/***************************************************************************

    Sega ST-V (Saturn) video: VDP1 and VDP2

    A C port of current MAME's Saturn video (sega/saturn.cpp, VDP1 and VDP2
    parts, and the CRTC of saturn_vdp2.cpp), by David Haywood, Angelo
    Salese, Olivier Galibert, Mariusz Wojcieszek and R. Belmont.

    - The VDP1 draws its command list into one of two 16-bit framebuffers,
      at vblank (automatic draw) or when the PTM register asks for it.
    - The VDP2 draws the back screen, the normal scroll screens NBG0-3, the
      rotation screens RBG0/RBG1 (with coefficient tables and windows), the
      line screen and the VDP1 framebuffer as the sprite layer, by priority,
      into a 32-bit RGB bitmap.
    - The tiles are decoded from the VDP2 VRAM into a cache when they are
      drawn after a write to them (MAME uses its gfx elements for this).

***************************************************************************/

#include "driver.h"
#include "state.h"
#include <stdarg.h>
#include "includes/stv.h"


/***************************************************************************
    Support for the MAME code: RGB, rectangles, bitmaps, palette, tiles
***************************************************************************/

/* rgb_t, RGB_RED/GREEN/BLUE and BIT come from the core */
#define RGB_MAKE(r,g,b)     ((((UINT32)(r) & 0xff) << 16) | (((UINT32)(g) & 0xff) << 8) | ((UINT32)(b) & 0xff))

/* memory handler byte lanes (a set mem_mask bit is a byte not accessed) */
#define ACCESSING_BITS_24_31    (!(mem_mask & 0xff000000))
#define ACCESSING_BITS_16_23    (!(mem_mask & 0x00ff0000))
#define ACCESSING_BITS_8_15     (!(mem_mask & 0x0000ff00))
#define ACCESSING_BITS_0_7      (!(mem_mask & 0x000000ff))
#define ACCESSING_BITS_16_31    (!(mem_mask & 0xffff0000))
#define ACCESSING_BITS_0_15     (!(mem_mask & 0x0000ffff))

#define BMP32(bmp, y)       ((UINT32 *)(bmp)->line[y])

static INLINE UINT32 stv_alpha_blend_r32(UINT32 d, UINT32 s, UINT8 level)
{
	return ((((s & 0x00ff00ff) * level + (d & 0x00ff00ff) * (256 - level)) >> 8) & 0x00ff00ff) |
		((((s & 0x0000ff00) * level + (d & 0x0000ff00) * (256 - level)) >> 8) & 0x0000ff00);
}

static INLINE UINT16 stv_alpha_blend_r16(UINT16 d, UINT16 s, UINT8 level)
{
	return ((((s & 0x7c1f) * level + (d & 0x7c1f) * (256 - level)) >> 8) & 0x7c1f) |
		((((s & 0x03e0) * level + (d & 0x03e0) * (256 - level)) >> 8) & 0x03e0);
}

static INLINE UINT32 add_blend_r32(UINT32 d, UINT32 s)
{
	UINT32 r = RGB_RED(d) + RGB_RED(s);
	UINT32 g = RGB_GREEN(d) + RGB_GREEN(s);
	UINT32 b = RGB_BLUE(d) + RGB_BLUE(s);
	if (r > 0xff) r = 0xff;
	if (g > 0xff) g = 0xff;
	if (b > 0xff) b = 0xff;
	return RGB_MAKE(r, g, b);
}

static INLINE INT32 mul_32x32_shift(INT32 a, INT32 b, int shift)
{
	return (INT32)(((INT64)a * (INT64)b) >> shift);
}

static void rect_set(struct rectangle *r, int min_x, int max_x, int min_y, int max_y)
{
	r->min_x = min_x;
	r->max_x = max_x;
	r->min_y = min_y;
	r->max_y = max_y;
}

static void rect_intersect(struct rectangle *r, const struct rectangle *with)
{
	if (r->min_x < with->min_x) r->min_x = with->min_x;
	if (r->max_x > with->max_x) r->max_x = with->max_x;
	if (r->min_y < with->min_y) r->min_y = with->min_y;
	if (r->max_y > with->max_y) r->max_y = with->max_y;
}

static void rect_clip_to_bitmap(struct rectangle *r, const struct mame_bitmap *bitmap)
{
	struct rectangle b;
	rect_set(&b, 0, bitmap->width - 1, 0, bitmap->height - 1);
	rect_intersect(r, &b);
}

static void bitmap_fill32(struct mame_bitmap *bitmap, const struct rectangle *clip, UINT32 color)
{
	int x, y;
	for (y = clip->min_y; y <= clip->max_y; y++)
	{
		UINT32 *line = BMP32(bitmap, y);
		for (x = clip->min_x; x <= clip->max_x; x++)
			line[x] = color;
	}
}

#define SWAP_INT(a,b)   do { INT32 swap_t_ = (a); (a) = (b); (b) = swap_t_; } while (0)
#define SWAP_PTR(a,b)   do { INT32 *swap_p_ = (a); (a) = (b); (b) = swap_p_; } while (0)

static void popmessage(const char *fmt, ...) { (void)fmt; }
static void log_masked(int mask, const char *fmt, ...) { (void)mask; (void)fmt; }

#define LOG_VDP2 (1U << 1)
#define LOG_ROZ  (1U << 2)

/*
	Threaded video: the screen can be drawn in horizontal bands by
	several threads (the "ST-V Threaded Video" core option). The state a
	layer keeps while it draws is per thread (VDP2_TLS), the rest is only
	read while the bands are drawn: the rotation plane caches are brought
	up to date before, by the main thread alone (VDP2_RENDER_PREPASS).
*/
/*
	As libretro-common (formats/data_transfer.c): no threaded video where
	there is no native thread-local storage (clang's __has_feature(tls):
	old Darwin deployment targets) or where GCC emulates it with a lookup
	per access (the consoles, and MinGW's GCC). Nor with MSVC before 2015,
	whose builds may run on Windows XP, where __declspec(thread) does not
	work in a DLL loaded at run time. The state is thread local even with
	the option off, so an emulated TLS would slow every game down.
*/
#if defined(HAVE_THREADS) && !defined(VITA) && !defined(PSP) && !defined(_3DS) && !defined(GEKKO) && \
	!(defined(__MINGW32__) && !defined(__clang__)) && !(defined(_MSC_VER) && _MSC_VER < 1900)
#define STV_VIDEO_THREADS 1
#if defined(__has_feature)
#if !__has_feature(tls)
#undef STV_VIDEO_THREADS
#endif
#endif
#endif

#ifdef STV_VIDEO_THREADS
#if defined(_MSC_VER)
#define VDP2_TLS __declspec(thread)
#elif defined(__GNUC__) && !defined(__APPLE__)
/* static TLS: no call to look it up at each access (the core is a shared */
/* object), it stays small (a few hundred bytes) to fit when it is loaded */
#define VDP2_TLS __thread __attribute__((tls_model("initial-exec")))
#else
#define VDP2_TLS __thread
#endif
#else
#define VDP2_TLS
#endif

/* for the core options: is the "ST-V Threaded Video" option of any use here */
int stv_video_threads_available(void)
{
#ifdef STV_VIDEO_THREADS
	return 1;
#else
	return 0;
#endif
}

enum
{
	VDP2_RENDER_NORMAL = 0,	/* one pass, caches and screen */
	VDP2_RENDER_PREPASS,	/* the rotation plane caches only */
	VDP2_RENDER_BAND		/* the screen only, the caches are up to date */
};
static int vdp2_render_mode;
#define VDP1_LOG 0

#define FRAC_SHIFT 16

#define RGB_R(color) ((color) & 0x1f)
#define RGB_G(color) (((color) >> 5) & 0x1f)
#define RGB_B(color) (((color) >> 10) & 0x1f)

/* the palette: CRAM colours (0-0x7ff), shadowed (0x800-), highlighted (0x1000-) */
static rgb_t stv_pens[2048 * 3];
#define PEN(n)      stv_pens[(n)]

static void set_pen(int n, rgb_t color)
{
	stv_pens[n] = color;
}

/* tiles of the VDP2 VRAM as MAME's four gfx elements: 8x8 and 16x16, 4 and 8
   bits per pixel, each element 32 bytes after the previous one (128 for the
   16x16 ones), decoded to one byte per pixel when drawn after a write */
struct stv_gfx
{
	int width, height, bpp;
	int elements;
	int inc;            /* bytes between elements */
	int size;           /* bytes of an element */
	int granularity;    /* colours per palette bank */
	int colors;         /* palette banks */
	UINT8 *cache;
	UINT8 *dirty;
};

static struct stv_gfx stv_gfx[4];

static const UINT8 *vdp2_gfx_source;

static void gfx_setup(struct stv_gfx *g, int width, int height, int bpp, int inc, int colors)
{
	g->width = width;
	g->height = height;
	g->bpp = bpp;
	g->inc = inc;
	g->size = width * height * bpp / 8;
	g->elements = 0x100000 / inc;
	g->granularity = 1 << bpp;
	g->colors = colors;
	g->cache = auto_malloc(g->elements * width * height);
	g->dirty = auto_malloc(g->elements);
	memset(g->dirty, 1, g->elements);
}

/* a VRAM byte at addr changed: the elements that contain it are stale */
static void gfx_mark_dirty_addr(UINT32 addr)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		struct stv_gfx *g = &stv_gfx[i];
		int last = addr / g->inc;
		int first = ((int)addr - g->size + 1) < 0 ? 0 : ((int)addr - g->size + 1 + g->inc - 1) / g->inc;
		int e;
		for (e = first; e <= last && e < g->elements; e++)
			g->dirty[e] = 1;
	}
}

static void gfx_decode_element(struct stv_gfx *g, int code)
{
	const UINT8 *src = vdp2_gfx_source + code * g->inc;
	UINT8 *dst = g->cache + code * g->width * g->height;
	int x, y;

	/* the 16x16 tiles are four 8x8 cells: left top, right top, left bottom, right bottom */
	for (y = 0; y < g->height; y++)
		for (x = 0; x < g->width; x++)
		{
			int cell = (g->width == 16) ? ((y >> 3) * 2 + (x >> 3)) : 0;
			int cx = x & 7, cy = y & 7;
			UINT32 offs;
			UINT8 pix;

			if (g->bpp == 4)
			{
				offs = cell * 32 + cy * 4 + (cx >> 1);
				pix = src[offs & 0xfffff];
				pix = (cx & 1) ? (pix & 0x0f) : (pix >> 4);
			}
			else
			{
				offs = cell * 64 + cy * 8 + cx;
				pix = src[offs & 0xfffff];
			}
			*dst++ = pix;
		}
}

#ifdef STV_VIDEO_THREADS
/*
	While the strips are drawn, a thread that finds an element stale
	decodes it itself and leaves it marked stale: another thread that
	reads it decodes it too, writing the same bytes, so no thread reads
	an element another one is writing. The elements decoded are noted
	per thread and marked clean once all the strips are drawn.
*/
#define VDP2_DECODED_MAX	16384
static VDP2_TLS int vdp2_band_index = -1;
static UINT32 *vdp2_band_decoded[4];
static int vdp2_band_decoded_count[4];
#endif

static INLINE const UINT8 *gfx_get_data(struct stv_gfx *g, int code)
{
	code %= g->elements;
	if (g->dirty[code])
	{
		gfx_decode_element(g, code);
#ifdef STV_VIDEO_THREADS
		if (vdp2_band_index >= 0)
		{
			int b = vdp2_band_index;
			if (vdp2_band_decoded[b] && vdp2_band_decoded_count[b] < VDP2_DECODED_MAX)
				vdp2_band_decoded[b][vdp2_band_decoded_count[b]++] = ((UINT32)(g - stv_gfx) << 24) | (UINT32)code;
		}
		else
#endif
			g->dirty[code] = 0;
	}
	return g->cache + code * g->width * g->height;
}

#define gfx_element struct stv_gfx


/***************************************************************************
    State
***************************************************************************/

UINT16 *vdp2_regs;
UINT32 *vdp2_vram;
UINT32 *vdp2_cram;
UINT32 *vdp1_vram;
UINT16 *vdp1_regs;

struct spoint
{
	INT32 x, y;
	INT32 u, v;
};

struct shaded_point
{
	INT32 x, y;
	INT32 r, g, b;
};

static struct
{
	UINT16 **framebuffer_display_lines;
	int framebuffer_mode;
	int framebuffer_double_interlace;
	int fbcr_accessed;
	int framebuffer_width;
	int framebuffer_height;
	int framebuffer_current_display;
	int framebuffer_current_draw;
	int framebuffer_clear_on_next_frame;
	struct rectangle system_cliprect;
	struct rectangle user_cliprect;
	UINT16 *framebuffer[2];
	UINT16 **framebuffer_draw_lines;
	UINT8 *gfx_decode;
	UINT16 lopr;
	UINT16 copr;
	UINT16 ewdr;

	int local_x;
	int local_y;

	void *draw_end_timer;
} vdp1;

static struct
{
	UINT8 *gfx_decode;
	struct mame_bitmap *roz_bitmap[2];
	int old_crmd;
} vdp2l;

struct vdp1_poly_scanline
{
	INT32 x[2];
	INT32 b[2];
	INT32 g[2];
	INT32 r[2];
	INT32 db;
	INT32 dg;
	INT32 dr;
};

struct vdp1_poly_scanline_data
{
	INT32 sy, ey;
	struct vdp1_poly_scanline scanline[512];
};

static struct vdp1_poly_scanline_data *vdp1_shading_data;

static struct
{
	int CMDCTRL, CMDLINK, CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDGRDA;
	int CMDXA, CMDYA;
	int CMDXB, CMDYB;
	int CMDXC, CMDYC;
	int CMDXD, CMDYD;

	int ispoly;
} current_sprite;

static struct
{
	UINT16 GA;
	UINT16 GB;
	UINT16 GC;
	UINT16 GD;
} gouraud_shading;

static UINT16 sprite_colorbank;

/* VDP1 framebuffer: the priorities used */
static VDP2_TLS int vdp1_sprite_priorities_used[8];
static VDP2_TLS int vdp1_sprite_priorities_usage_valid;
static UINT8 vdp1_sprite_priorities_in_fb_line[512][8];
/* the first and last framebuffer x of the pixels of each priority seen on a line */
static UINT16 vdp1_sprite_priorities_min_x[512][8], vdp1_sprite_priorities_max_x[512][8];

/* a pixel of priority 'p' at (x, y) another pass draws */
#define SPRITE_PRIORITY_SEEN(p, x, y) do { \
	UINT8 sp_ = (p); \
	vdp1_sprite_priorities_used[sp_] = 1; \
	vdp1_sprite_priorities_in_fb_line[y][sp_] = 1; \
	if ((x) < vdp1_sprite_priorities_min_x[y][sp_]) vdp1_sprite_priorities_min_x[y][sp_] = (x); \
	if ((x) > vdp1_sprite_priorities_max_x[y][sp_]) vdp1_sprite_priorities_max_x[y][sp_] = (x); \
} while (0)

struct vdp2_tilemap_capabilities
{
	UINT8  enabled;
	UINT8  transparency;
	UINT8  colour_calculation_enabled;
	UINT8  colour_depth;
	UINT8  alpha;
	UINT8  tile_size;
	UINT8  bitmap_enable;
	UINT8  bitmap_size;
	UINT8  bitmap_palette_number;
	UINT8  bitmap_map;
	UINT16 map_offset[16];
	UINT8  map_count;

	UINT8  pattern_data_size;
	UINT8  character_number_supplement;
	UINT8  special_priority_register;
	UINT8  special_colour_control_register;
	UINT8  supplementary_palette_bits;
	UINT8  supplementary_character_bits;

	INT16 scrollx;
	INT16 scrolly;
	UINT32 incx, incy;

	UINT8   linescroll_enable;
	UINT8   linescroll_interval;
	UINT32  linescroll_table_address;
	UINT8   vertical_linescroll_enable;
	UINT8   vertical_cell_scroll_enable;
	UINT8   linezoom_enable;

	UINT8  plane_size;
	UINT8  colour_ram_address_offset;
	UINT8  fade_control;
	struct
	{
		UINT8 logic;
		UINT8 enabled[2];
		UINT8 area[2];
	} window_control;

	UINT8  line_screen_enabled;
	UINT8  mosaic_screen_enabled;
	int roz_mode3;

	int layer_name; /* just to keep track */
};

static VDP2_TLS struct vdp2_tilemap_capabilities current_tilemap;

static VDP2_TLS struct
{
	INT32   xst;
	INT32   yst;
	INT32   zst;
	INT32   dxst;
	INT32   dyst;
	INT32   dx;
	INT32   dy;
	INT32   A;
	INT32   B;
	INT32   C;
	INT32   D;
	INT32   E;
	INT32   F;
	INT32   px;
	INT32   py;
	INT32   pz;
	INT32   cx;
	INT32   cy;
	INT32   cz;
	INT32   mx;
	INT32   my;
	INT32   kx;
	INT32   ky;
	UINT32  kast;
	INT32   dkast;
	INT32   dkax;
} current_rotation_table;

static VDP2_TLS struct
{
	UINT32  map_offset_min;
	UINT32  map_offset_max;
	UINT32  tile_offset_min;
	UINT32  tile_offset_max;
	UINT32  pen_min;
	UINT32  pen_max;
} vdp2_layer_data;

static struct
{
	UINT8   watch_vdp2_vram_writes;
	UINT8   is_cache_dirty;

	UINT32  map_offset_min[2];
	UINT32  map_offset_max[2];
	UINT32  tile_offset_min[2];
	UINT32  tile_offset_max[2];
	UINT32  pen_min[2];
	UINT32  pen_max[2];

	/* pattern name words written since the last update, redrawn without a full rebuild */
	UINT32  map_dirty_min[2];
	UINT32  map_dirty_max[2];

	struct vdp2_tilemap_capabilities layer_data[2];
} RBG0_cache_data;

/* set while vdp2_draw_basic_tilemap only redraws the cells whose pattern name is in [min, max) */
static struct
{
	int     active;
	UINT32  min, max;
} vdp2_cell_redraw;

static UINT8 m_vdpdebug_roz;

/* the VDP2 CRTC (saturn_vdp2.cpp) */
static struct
{
	UINT16 tvmd, old_tvmd;
	int disp, bdclmd, lsmd, vreso, hreso;
	int odd_bit;
	UINT16 exten;
	int exlten, exsyen, dasel, exbgen;
	int exltfg, exsyfg;
	UINT16 hcounter_latch, vcounter_latch;
	int vramsz;
	int hdisplay, vdisplay;
	int is_pal;
} crtc;

#define get_lsmd()      (crtc.lsmd)
#define get_hreso()     (crtc.hreso)
#define get_vramsz()    (crtc.vramsz)
#define get_disp()      (crtc.disp)
#define get_bdclmd()    (crtc.bdclmd)

static void (*drawpixel)(int x, int y, int patterndata, int offsetcnt);

static void CEF_1(void) { vdp1_regs[0x010/2] |= 0x0002; }
static void CEF_0(void) { vdp1_regs[0x010/2] &= ~0x0002; }
static void BEF_1(void) { vdp1_regs[0x010/2] |= 0x0001; }
static void BEF_0(void) { vdp1_regs[0x010/2] &= ~0x0001; }
#define VDP1_TVMR()     (vdp1_regs[0x000/2] & 0xffff)
#define VDP1_VBE()      ((VDP1_TVMR() & 0x0008) >> 3)
#define VDP1_TVM()      ((VDP1_TVMR() & 0x0007) >> 0)

/*TV Mode Selection Register */
/*
   xxxx xxxx xxxx ---- | UNUSED
   ---- ---- ---- x--- | VBlank Erase/Write (VBE)
   ---- ---- ---- -xxx | TV Mode (TVM)
   TV-Mode:
   This sets the Frame Buffer size,the rotation of the Frame Buffer & the bit width.
   bit 2 HDTV disable(0)/enable(1)
   bit 1 non-rotation/rotation(1)
   bit 0 16(0)/8(1) bits per pixel
   Size of the Frame Buffer:
   7 invalid
   6 invalid
   5 invalid
   4 512x256
   3 512x512
   2 512x256
   1 1024x256
   0 512x256
*/

/*Frame Buffer Change Mode Register*/
/*
   xxxx xxxx xxx- ---- | UNUSED
   ---- ---- ---x ---- | Even/Odd Coordinate Select Bit (EOS)
   ---- ---- ---- x--- | Double Interlace Mode (DIE)
   ---- ---- ---- -x-- | Double Interlace Draw Line (DIL)
   ---- ---- ---- --x- | Frame Buffer Change Trigger (FCM)
   ---- ---- ---- ---x | Frame Buffer Change Mode (FCT)
*/
#define VDP1_FBCR ((vdp1_regs[0x002/2] >> 0)&0xffff)
#define VDP1_EOS ((VDP1_FBCR & 0x0010) >> 4)
#define VDP1_DIE ((VDP1_FBCR & 0x0008) >> 3)
#define VDP1_DIL ((VDP1_FBCR & 0x0004) >> 2)
#define VDP1_FCM ((VDP1_FBCR & 0x0002) >> 1)
#define VDP1_FCT ((VDP1_FBCR & 0x0001) >> 0)

/*Plot Trigger Register*/
/*
   xxxx xxxx xxxx xx-- | UNUSED
   ---- ---- ---- --xx | Plot Trigger Mode (PTM)

   Plot Trigger Mode:
   3 Invalid
   2 Automatic draw
   1 VDP1 draw by request
   0 VDP1 Idle (no access)
*/
#define VDP1_PTMR ((vdp1_regs[0x004/2])&0xffff)
#define VDP1_PTM  ((VDP1_PTMR & 0x0003) >> 0)
#define PTM_0         vdp1_regs[0x004/2]&=~0x0001

/*
    Erase/Write Data Register
    16 bpp = data
    8 bpp = erase/write data for even/odd X coordinates
*/
#define VDP1_EWDR ((vdp1_regs[0x006/2])&0xffff)

/*Erase/Write Upper-Left register*/
/*
   x--- ---- ---- ---- | UNUSED
   -xxx xxx- ---- ---- | X1 register
   ---- ---x xxxx xxxx | Y1 register

*/
#define VDP1_EWLR ((vdp1_regs[0x008/2])&0xffff)
#define VDP1_EWLR_X1 ((VDP1_EWLR & 0x7e00) >> 9)
#define VDP1_EWLR_Y1 ((VDP1_EWLR & 0x01ff) >> 0)
/*Erase/Write Lower-Right register*/
/*
   xxxx xxx- ---- ---- | X3 register
   ---- ---x xxxx xxxx | Y3 register

*/
#define VDP1_EWRR ((vdp1_regs[0x00a/2])&0xffff)
#define VDP1_EWRR_X3 ((VDP1_EWRR & 0xfe00) >> 9)
#define VDP1_EWRR_Y3 ((VDP1_EWRR & 0x01ff) >> 0)
/*Transfer End Status Register*/
/*
   xxxx xxxx xxxx xx-- | UNUSED
   ---- ---- ---- --x- | CEF
   ---- ---- ---- ---x | BEF

*/
#define VDP1_EDSR ((vdp1_regs[0x010/2])&0xffff)
#define VDP1_CEF  (VDP1_EDSR & 2)
#define VDP1_BEF  (VDP1_EDSR & 1)
/**/



/* bumped at each screen update, see vdp2_get_window0_coordinates */
static int vdp2_window_cache_gen;
#define VDP2_WINDOW_LINES	1024
static struct vdp2_window_coord { int gen, sx, ex, sy, ey; } vdp2_window_coords[2][VDP2_WINDOW_LINES];

/* prototypes */
static void vdp2_get_window0_coordinates_calc(int *s_x, int *e_x, int *s_y, int *e_y, int y);
static void vdp2_get_window1_coordinates_calc(int *s_x, int *e_x, int *s_y, int *e_y, int y);
static UINT16 vdp1_regs_r(offs_t offset);
static void vdp1_clear_framebuffer( int which_framebuffer );
static void vdp1_prepare_framebuffers(void);
static void vdp1_change_framebuffers(void);
static void vdp1_set_framebuffer_config(void);
static void vdp1_regs_w(offs_t offset, UINT16 data, UINT16 mem_mask);
static UINT32 vdp1_vram_r(offs_t offset);
static void vdp1_vram_w(offs_t offset, UINT32 data, UINT32 mem_mask);
static void vdp1_framebuffer0_w(offs_t offset, UINT32 data, UINT32 mem_mask);
static UINT32 vdp1_framebuffer0_r(offs_t offset, UINT32 mem_mask);
static void clear_gouraud_shading(void);
static UINT8 read_gouraud_table(void);
static INLINE INT32 _shading( INT32 color, INT32 correction );
static UINT16 vdp1_apply_gouraud_shading( int x, int y, UINT16 pix );
static void vdp1_setup_shading_for_line( INT32 y, INT32 x1, INT32 x2, INT32 r1, INT32 g1, INT32 b1, INT32 r2, INT32 g2, INT32 b2);
static void vdp1_setup_shading_for_slope( INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 *nx1, INT32 *nx2, INT32 r1, INT32 r2, INT32 slr1, INT32 slr2, INT32 *nr1, INT32 *nr2, INT32 g1, INT32 g2, INT32 slg1, INT32 slg2, INT32 *ng1, INT32 *ng2, INT32 b1, INT32 b2, INT32 slb1, INT32 slb2, INT32 *nb1, INT32 *nb2, INT32 _y1, INT32 y2);
static void vdp1_setup_shading(const struct spoint* q, const struct rectangle *cliprect);
static void drawpixel_poly(int x, int y, int patterndata, int offsetcnt);
static void drawpixel_8bpp_trans(int x, int y, int patterndata, int offsetcnt);
static void drawpixel_4bpp_notrans(int x, int y, int patterndata, int offsetcnt);
static void drawpixel_4bpp_trans(int x, int y, int patterndata, int offsetcnt);
static void drawpixel_generic(int x, int y, int patterndata, int offsetcnt);
static void vdp1_set_drawpixel(void);
static void vdp1_fill_slope(const struct rectangle *cliprect, int patterndata, int xsize, INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 *nx1, INT32 *nx2, INT32 u1, INT32 u2, INT32 slu1, INT32 slu2, INT32 *nu1, INT32 *nu2, INT32 v1, INT32 v2, INT32 slv1, INT32 slv2, INT32 *nv1, INT32 *nv2, INT32 _y1, INT32 y2);
static void vdp1_fill_line(const struct rectangle *cliprect, int patterndata, int xsize, INT32 y, INT32 x1, INT32 x2, INT32 u1, INT32 u2, INT32 v1, INT32 v2);
static void vdp1_fill_quad(const struct rectangle *cliprect, int patterndata, int xsize, const struct spoint *q);
static int x2s(int v);
static int y2s(int v);
static void vdp1_draw_line(const struct rectangle *cliprect);
static void vdp1_draw_poly_line(const struct rectangle *cliprect);
static void vdp1_draw_distorted_sprite(const struct rectangle *cliprect);
static void vdp1_draw_scaled_sprite(const struct rectangle *cliprect);
static void vdp1_draw_normal_sprite(const struct rectangle *cliprect, int sprite_type);
static void vdp1_draw_end(int param);
static void vdp1_process_list(void);
static void vdp1_video_update(void);
static void vdp1_state_save_postload(void);
static int vdp1_start(void);
static void vdp2_fill_rotation_parameter_table( UINT8 rot_parameter );
static UINT8 vdp2_is_rotation_applied(void);
static UINT8 vdp2_are_map_registers_equal(void);
static void vdp2_check_fade_control_for_layer(void);
static UINT8 vdp2_check_vram_cycle_pattern_registers( UINT8 access_command_pnmdr, UINT8 access_command_cpdr, UINT8 bitmap_enable );
static void vdp2_compute_color_offset( int *r, int *g, int *b, int cor );
static void vdp2_drawgfxzoom( struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx, UINT32 code,UINT32 color,int flipx,int flipy,int sx,int sy, int transparency,int scalex, int scaley, int sprite_screen_width, int sprite_screen_height, int alpha);
static void vdp2_drawgfxzoom_rgb555( struct mame_bitmap *dest_bmp,const struct rectangle *clip, UINT32 code,UINT32 color,int flipx,int flipy,int sx,int sy, int transparency,int scalex, int scaley, int sprite_screen_width, int sprite_screen_height, int alpha);
static void vdp2_drawgfx_rgb555(struct mame_bitmap *dest_bmp, const struct rectangle *clip, UINT32 code, int flipx, int flipy, int sx, int sy, int transparency, int alpha);
static void vdp2_drawgfx_rgb888( struct mame_bitmap *dest_bmp, const struct rectangle *clip, UINT32 code, int flipx, int flipy, int sx, int sy, int transparency, int alpha);
static void vdp2_drawgfx_alpha(struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx, UINT32 code,UINT32 color, int flipx,int flipy,int offsx,int offsy, int transparency, int alpha);
static void vdp2_drawgfx_transpen(struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx, UINT32 code,UINT32 color, int flipx,int flipy,int offsx,int offsy, int transparency);
static void draw_4bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void draw_8bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void draw_11bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void draw_rgb15_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void draw_rgb32_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_basic_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_get_map_page( int x, int y, int *_map, int *_page );
static void vdp2_draw_basic_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_check_tilemap_with_linescroll(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_line(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_mosaic(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 is_roz);
static void vdp2_check_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_copy_roz_bitmap(struct mame_bitmap *bitmap, struct mame_bitmap *roz_bitmap, const struct rectangle *cliprect, int iRP, int planesizex, int planesizey, int planerenderedsizex, int planerenderedsizey);
static INLINE int get_roz_window_pixel(int s_x,int e_x,int s_y,int e_y,int x, int y,UINT8 winenable, UINT8 winarea);
static void vdp2_draw_NBG0(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_NBG1(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_NBG2(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_NBG3(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_rotation_screen(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int iRP);
static void vdp2_draw_RBG0(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void vdp2_draw_back(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static UINT32 vdp2_vram_r(offs_t offset);
static void vdp2_vram_w(offs_t offset, UINT32 data, UINT32 mem_mask);
static UINT16 vdp2_regs_r(offs_t offset);
static UINT32 vdp2_cram_r(offs_t offset);
static void vdp2_cram_w(offs_t offset, UINT32 data, UINT32 mem_mask);
static void refresh_palette_data(void);
static void vdp2_regs_w(offs_t offset, UINT16 data, UINT16 mem_mask);
static void vdp2_state_save_postload(void);
static int vdp2_start(void);
static void vdp2_fade_effects(void);
static INLINE void vdp2_get_window0_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y);
static INLINE void vdp2_get_window1_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y);
static INLINE int vdp2_window_process(int x,int y);
static int vdp2_apply_window_on_layer(struct rectangle *cliprect);
static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 pri);
static void vdp2_render(struct mame_bitmap *bitmap, const struct rectangle *cliprect);
static void crtc_init_vcounter_table(void);
static int crtc_htotal(void);
static void crtc_reconfigure(void);
static int crtc_hpos(void);
static int crtc_get_hcounter(void);
static int crtc_get_vcounter(void);
static int crtc_get_hblank(void);
static int crtc_get_vblank(void);
static void crtc_tvmd_w(UINT16 data, UINT16 mem_mask);
static UINT16 crtc_regs_r(int offset);
static void crtc_regs_w(int offset, UINT16 data, UINT16 mem_mask);
static void crtc_state_save_postload(void);
static void crtc_start(void);

static UINT16 vdp1_regs_r(offs_t offset)
{
	/*logerror ("%s VDP1: Read from Registers, Offset %04x\n", machine().describe_context(), offset); */

	switch(offset)
	{
		case 0x02/2:
			return 0;
		case 0x10/2:
			break;
		case 0x12/2: return vdp1.lopr;
		case 0x14/2: return vdp1.copr;
		/* MODR register, read register for the other VDP1 regs
		   (Shienryu SS version abuses of this during intro) */
		case 0x16/2:
		{
			UINT16 modr;

			modr = 0x1000; /*vdp1 VER */
			modr |= (VDP1_PTM >> 1) << 8; /* PTM1 */
			modr |= VDP1_EOS << 7; /* EOS */
			modr |= VDP1_DIE << 6; /* DIE */
			modr |= VDP1_DIL << 5; /* DIL */
			modr |= VDP1_FCM << 4; /*FCM */
			modr |= VDP1_VBE() << 3; /*VBE */
			modr |= VDP1_TVM() & 7; /*TVM */

			return modr;
		}
		default:
			break;
	}

	return vdp1_regs[offset]; /*TODO: write-only regs should return open bus or zero */
}

/* n pixels of value v from p, eight bytes at a time where p allows it */
static void vdp1_fill16(UINT16 *p, int n, UINT16 v)
{
	while (n > 0 && ((size_t)p & 7))
	{
		*p++ = v;
		n--;
	}
	if (n >= 4)
	{
		UINT64 v4 = (UINT64)v * (((UINT64)1 << 48) | ((UINT64)1 << 32) | ((UINT64)1 << 16) | 1);
		int i, n4 = n >> 2;
		/* memcpy: no aliasing of the UINT16 frame buffer as UINT64 */
		for (i = 0; i < n4; i++)
			memcpy(p + (i << 2), &v4, 8);
		p += n4 << 2;
		n &= 3;
	}
	while (n-- > 0)
		*p++ = v;
}

/* TODO: TVM & 1 is just a kludgy work-around, the VDP1 actually needs to be rewritten from scratch. */
/* Daisenryaku Strong Style (daisenss) uses it */
static void vdp1_clear_framebuffer( int which_framebuffer )
{
	int y;
	int start_x, end_x, start_y, end_y;

	start_x = VDP1_EWLR_X1 * ((VDP1_TVM() & 1) ? 16 : 8);
	start_y = VDP1_EWLR_Y1 * (vdp1.framebuffer_double_interlace+1);
	end_x = VDP1_EWRR_X3 * ((VDP1_TVM() & 1) ? 16 : 8);
	end_y = (VDP1_EWRR_Y3+1) * (vdp1.framebuffer_double_interlace+1);
/*  popmessage("%d %d %d %d %d",VDP1_EWLR_X1,VDP1_EWLR_Y1,VDP1_EWRR_X3,VDP1_EWRR_Y3,vdp1.framebuffer_double_interlace); */

	/* the pixels (x & mask, y & 511) for x in [start_x, end_x): one or two
	   runs of a row, the same pixels as a loop on x */
	{
		const int width = (VDP1_TVM() & 1) ? 1024 : 512;
		const int count = end_x - start_x;
		const UINT16 value = vdp1.ewdr;
		int first, second, x0;

		if (count >= width)
		{
			x0 = 0; first = width; second = 0;
		}
		else
		{
			x0 = start_x & (width - 1);
			first = (count > width - x0) ? width - x0 : count;
			second = count - first;
		}
		if (count > 0)
			for(y=start_y;y<end_y;y++)
			{
				UINT16 *row = vdp1.framebuffer[ which_framebuffer ] + (y&511)*width;
				vdp1_fill16(row + x0, first, value);
				vdp1_fill16(row, second, value);
			}
	}

	if ( VDP1_LOG ) logerror( "Clearing %d framebuffer\n", vdp1.framebuffer_current_draw );
/*  memset( vdp1.framebuffer[ which_framebuffer ], vdp1.ewdr, 1024 * 256 * sizeof(UINT16) * 2 ); */
}


static void vdp1_prepare_framebuffers(void)
{
	int i,rowsize;

	rowsize = vdp1.framebuffer_width;
	if ( vdp1.framebuffer_current_draw == 0 )
	{
		for ( i = 0; i < vdp1.framebuffer_height; i++ )
		{
			vdp1.framebuffer_draw_lines[i] = &vdp1.framebuffer[0][ i * rowsize ];
			vdp1.framebuffer_display_lines[i] = &vdp1.framebuffer[1][ i * rowsize ];
		}
		for ( ; i < 512; i++ )
		{
			vdp1.framebuffer_draw_lines[i] = &vdp1.framebuffer[0][0];
			vdp1.framebuffer_display_lines[i] = &vdp1.framebuffer[1][0];
		}
	}
	else
	{
		for ( i = 0; i < vdp1.framebuffer_height; i++ )
		{
			vdp1.framebuffer_draw_lines[i] = &vdp1.framebuffer[1][ i * rowsize ];
			vdp1.framebuffer_display_lines[i] = &vdp1.framebuffer[0][ i * rowsize ];
		}
		for ( ; i < 512; i++ )
		{
			vdp1.framebuffer_draw_lines[i] = &vdp1.framebuffer[1][0];
			vdp1.framebuffer_display_lines[i] = &vdp1.framebuffer[0][0];
		}

	}

	for ( ; i < 512; i++ )
	{
		vdp1.framebuffer_draw_lines[i] = &vdp1.framebuffer[0][0];
		vdp1.framebuffer_display_lines[i] = &vdp1.framebuffer[1][0];
	}

}

static void vdp1_change_framebuffers(void)
{
	vdp1.framebuffer_current_display ^= 1;
	vdp1.framebuffer_current_draw ^= 1;
	/* "this bit is reset to 0 when the frame buffers are changed" */
	CEF_0();
	if ( VDP1_LOG ) logerror( "Changing framebuffers: %d - draw, %d - display\n", vdp1.framebuffer_current_draw, vdp1.framebuffer_current_display );
	vdp1_prepare_framebuffers();
}

static void vdp1_set_framebuffer_config(void)
{
	if ( vdp1.framebuffer_mode == VDP1_TVM() &&
			vdp1.framebuffer_double_interlace == VDP1_DIE ) return;

	if ( VDP1_LOG ) logerror( "Setting framebuffer config\n" );
	vdp1.framebuffer_mode = VDP1_TVM();
	vdp1.framebuffer_double_interlace = VDP1_DIE;
	switch( vdp1.framebuffer_mode )
	{
		case 0: vdp1.framebuffer_width = 512; vdp1.framebuffer_height = 256; break;
		case 1: vdp1.framebuffer_width = 1024; vdp1.framebuffer_height = 256; break;
		case 2: vdp1.framebuffer_width = 512; vdp1.framebuffer_height = 256; break;
		case 3: vdp1.framebuffer_width = 512; vdp1.framebuffer_height = 512; break;
		case 4: vdp1.framebuffer_width = 512; vdp1.framebuffer_height = 256; break;
		default: logerror( "Invalid framebuffer config %x\n", VDP1_TVM() ); vdp1.framebuffer_width = 512; vdp1.framebuffer_height = 256; break;
	}
	if ( VDP1_DIE ) vdp1.framebuffer_height *= 2; /* double interlace */

	vdp1.framebuffer_current_draw = 0;
	vdp1.framebuffer_current_display = 1;
	vdp1_prepare_framebuffers();
}

static void vdp1_regs_w(offs_t offset, UINT16 data, UINT16 mem_mask)
{
	/* PTM = 01 starts drawing immediately, while 00/10 only change the plot trigger mode at the */
	/* next frame buffer change, so a 01 write leaves the mode from the last 00/10 write in effect. */
	/* - doom and exhumed set 10 then draw with 01, and hang unless the next change draws again (CEF) */
	if (offset == 0x04/2 && ACCESSING_BITS_0_7 && (data & 3) == 1)
	{
		if ( VDP1_LOG ) logerror( "VDP1: Access to register PTMR = %1X\n", data );
		vdp1_process_list();
		return;
	}

	COMBINE_DATA(&vdp1_regs[offset]);

	switch(offset)
	{
		case 0x00/2:
			vdp1_set_framebuffer_config();
			if ( VDP1_LOG ) logerror( "VDP1: Access to register TVMR = %1X\n", data );

			break;
		case 0x02/2:
			vdp1_set_framebuffer_config();
			if ( VDP1_LOG ) logerror( "VDP1: Access to register FBCR = %1X\n", data );
			vdp1.fbcr_accessed = 1;
			break;
		case 0x04/2:
			if ( VDP1_LOG ) logerror( "VDP1: Access to register PTMR = %1X\n", data );
			break;
		case 0x06/2:
			if ( VDP1_LOG ) logerror( "VDP1: Erase data set %08X\n", data );

			vdp1.ewdr = VDP1_EWDR;
			break;
		case 0x08/2:
			if ( VDP1_LOG ) logerror( "VDP1: Erase upper-left coord set: %08X\n", data );
			break;
		case 0x0a/2:
			if ( VDP1_LOG ) logerror( "VDP1: Erase lower-right coord set: %08X\n", data );
			break;
		case 0x0c/2:
		case 0x0e/2: /* After Burner 2 / Out Run / Fantasy Zone writes here with a dword ... */
			if ( VDP1_LOG ) logerror( "VDP1: Draw forced termination register write: %08X %08X\n", offset*2, data );
			break;
		default:
			logerror("Warning: write to unknown VDP1 reg %08x %08x\n",offset*2,data);
			break;
	}

}

static UINT32 vdp1_vram_r(offs_t offset)
{
	return vdp1_vram[offset];
}


static void vdp1_vram_w(offs_t offset, UINT32 data, UINT32 mem_mask)
{
	UINT8 *gfx = vdp1.gfx_decode;

	COMBINE_DATA (&vdp1_vram[offset]);

/*  if (((offset * 4) > 0xdf) && ((offset * 4) < 0x140)) */
/*  { */
/*      logerror("%s: VRAM dword write to %08X = %08X & %08X\n", machine().describe_context(), offset*4, data, mem_mask); */
/*  } */

	data = vdp1_vram[offset];
	/* put in gfx region for easy decoding */
	gfx[offset*4+0] = (data & 0xff000000) >> 24;
	gfx[offset*4+1] = (data & 0x00ff0000) >> 16;
	gfx[offset*4+2] = (data & 0x0000ff00) >> 8;
	gfx[offset*4+3] = (data & 0x000000ff) >> 0;
}

static void vdp1_framebuffer0_w(offs_t offset, UINT32 data, UINT32 mem_mask)
{
	/*popmessage ("STV VDP1 Framebuffer 0 WRITE offset %08x data %08x",offset, data); */
	if ( VDP1_TVM() & 1 )
	{
		/* 8-bit mode */
		/*printf("VDP1 8-bit mode %08x %02x\n",offset,data); */
		if ( ACCESSING_BITS_24_31 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] &= 0x00ff;
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] |= data & 0xff00;
		}
		if ( ACCESSING_BITS_16_23 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] &= 0xff00;
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] |= data & 0x00ff;
		}
		if ( ACCESSING_BITS_8_15 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] &= 0x00ff;
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] |= data & 0xff00;
		}
		if ( ACCESSING_BITS_0_7 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] &= 0xff00;
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] |= data & 0x00ff;
		}
	}
	else
	{
		/* 16-bit mode */
		if ( ACCESSING_BITS_16_31 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] = (data >> 16) & 0xffff;
		}
		if ( ACCESSING_BITS_0_15 )
		{
			vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] = data & 0xffff;
		}
	}
}

static UINT32 vdp1_framebuffer0_r(offs_t offset, UINT32 mem_mask)
{
	UINT32 result = 0;
	/*popmessage ("STV VDP1 Framebuffer 0 READ offset %08x",offset); */
	if ( VDP1_TVM() & 1 )
	{
		/* 8-bit mode */
		/*printf("VDP1 8-bit mode %08x\n",offset); */
		if ( ACCESSING_BITS_24_31 )
			result |= ((vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] & 0xff00) << 16);
		if ( ACCESSING_BITS_16_23 )
			result |= ((vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] & 0x00ff) << 16);
		if ( ACCESSING_BITS_8_15 )
			result |= ((vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] & 0xff00));
		if ( ACCESSING_BITS_0_7 )
			result |= ((vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1] & 0x00ff));
	}
	else
	{
		/* 16-bit mode */
		if ( ACCESSING_BITS_16_31 )
		{
			result |= (vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2] << 16);
		}
		if ( ACCESSING_BITS_0_15 )
		{
			result |= (vdp1.framebuffer[vdp1.framebuffer_current_draw][offset*2+1]);
		}

	}

	return result;
}


/*

there is a command every 0x20 bytes
the first word is the control word
the rest are data used by it

---
00 CMDCTRL
   e--- ---- ---- ---- | end bit (15)
   -jjj ---- ---- ---- | jump select bits (12-14)
   ---- zzzz ---- ---- | zoom point / hotspot (8-11)
   ---- ---- 00-- ---- | UNUSED
   ---- ---- --dd ---- | character read direction (4,5)
   ---- ---- ---- cccc | command bits (0-3)

02 CMDLINK
   llll llll llll ll-- | link
   ---- ---- ---- --00 | UNUSED

04 CMDPMOD
   m--- ---- ---- ---- | MON (looks at MSB and apply shadows etc.)
   -00- ---- ---- ---- | UNUSED
   ---h ---- ---- ---- | HSS (High Speed Shrink)
   ---- p--- ---- ---- | PCLIP (Pre Clipping Disable)
   ---- -c-- ---- ---- | CLIP (Clipping Mode Bit)
   ---- --m- ---- ---- | CMOD (User Clipping Enable Bit)
   ---- ---M ---- ---- | MESH (Mesh Enable Bit)
   ---- ---- e--- ---- | ECD (End Code Disable)
   ---- ---- -S-- ---- | SPD (Transparent Pixel Disable)
   ---- ---- --cc c--- | Colour Mode
   ---- ---- ---- -CCC | Colour Calculation bits
   ---- ---- ---- -1-- | Gouraud shading enable
   ---- ---- ---- --1- | 1/2 original GFX enable
   ---- ---- ---- ---1 | 1/2 background enable

06 CMDCOLR
   mmmm mmmm mmmm mmmm | Colour Bank, Colour Lookup /8

08 CMDSRCA (Character Address)
   aaaa aaaa aaaa aa-- | Character Address
   ---- ---- ---- --00 | UNUSED

0a CMDSIZE (Character Size)
   00-- ---- ---- ---- | UNUSED
   --xx xxxx ---- ---- | Character Size (X)
   ---- ---- yyyy yyyy | Character Size (Y)

0c CMDXA (used for normal sprite)
   eeee ee-- ---- ---- | extension bits
   ---- --xx xxxx xxxx | x position

0e CMDYA (used for normal sprite)
   eeee ee-- ---- ---- | extension bits
   ---- --yy yyyy yyyy | y position

10 CMDXB
12 CMDYB
14 CMDXC
16 CMDYC
18 CMDXD
1a CMDYD
1c CMDGRDA (Gouraud Shading Table)
1e UNUSED
---


*/

static void clear_gouraud_shading(void)
{
	memset(&gouraud_shading, 0, sizeof(gouraud_shading));
}

static UINT8 read_gouraud_table(void)
{
	int gaddr;

	if ( current_sprite.CMDPMOD & 0x4 )
	{
		gaddr = current_sprite.CMDGRDA * 8;
		gouraud_shading.GA = (vdp1_vram[gaddr/4] >> 16) & 0xffff;
		gouraud_shading.GB = (vdp1_vram[gaddr/4] >> 0) & 0xffff;
		gouraud_shading.GC = (vdp1_vram[gaddr/4 + 1] >> 16) & 0xffff;
		gouraud_shading.GD = (vdp1_vram[gaddr/4 + 1] >> 0) & 0xffff;
		return 1;
	}
	else
	{
		return 0;
	}
}

static INLINE INT32 _shading( INT32 color, INT32 correction )
{
	correction = (correction >> 16) & 0x1f;
	color += (correction - 16);

	if ( color < 0 ) color = 0;
	if ( color > 0x1f ) color = 0x1f;

	return color;
}

static UINT16 vdp1_apply_gouraud_shading( int x, int y, UINT16 pix )
{
	INT32 r,g,b, msb;

	msb = pix & 0x8000;

#ifdef MAME_DEBUG
	if ( (vdp1_shading_data->scanline[y].x[0] >> 16) != x )
	{
		logerror( "ERROR in computing x coordinates (line %d, x = %x, %d, xc = %x, %d)\n", y, x, x, vdp1_shading_data->scanline[y].x[0], vdp1_shading_data->scanline[y].x[0] >> 16 );
	};
#endif

	b = RGB_B(pix);
	g = RGB_G(pix);
	r = RGB_R(pix);

	b = _shading( b, vdp1_shading_data->scanline[y].b[0] );
	g = _shading( g, vdp1_shading_data->scanline[y].g[0] );
	r = _shading( r, vdp1_shading_data->scanline[y].r[0] );

	vdp1_shading_data->scanline[y].b[0] += vdp1_shading_data->scanline[y].db;
	vdp1_shading_data->scanline[y].g[0] += vdp1_shading_data->scanline[y].dg;
	vdp1_shading_data->scanline[y].r[0] += vdp1_shading_data->scanline[y].dr;

	vdp1_shading_data->scanline[y].x[0] += 1 << FRAC_SHIFT;

	return msb | b << 10 | g << 5 | r;
}

static void vdp1_setup_shading_for_line(
		INT32 y, INT32 x1, INT32 x2,
		INT32 r1, INT32 g1, INT32 b1,
		INT32 r2, INT32 g2, INT32 b2)
{
	int xx1 = x1>>FRAC_SHIFT;
	int xx2 = x2>>FRAC_SHIFT;


	if ( xx1 > xx2 )
	{
		SWAP_INT(xx1, xx2);
		SWAP_INT(r1, r2);
		SWAP_INT(g1, g2);
		SWAP_INT(b1, b2);
	}

	if ( (y >= 0) && (y < 512) )
	{
		INT32  dx;
		INT32   gbd, ggd, grd;

		dx = xx2 - xx1;

		if ( dx == 0 )
		{
			gbd = ggd = grd = 0;
		}
		else
		{
			gbd = abs(b2 - b1) / dx;
			if (b2 < b1) gbd = -gbd;
			ggd = abs(g2 - g1) / dx;
			if (g2 < g1) ggd = -ggd;
			grd = abs(r2 - r1) / dx;
			if (r2 < r1) grd = -grd;
		}

		vdp1_shading_data->scanline[y].x[0] = x1;
		vdp1_shading_data->scanline[y].x[1] = x2;

		vdp1_shading_data->scanline[y].b[0] = b1;
		vdp1_shading_data->scanline[y].g[0] = g1;
		vdp1_shading_data->scanline[y].r[0] = r1;
		vdp1_shading_data->scanline[y].b[1] = b2;
		vdp1_shading_data->scanline[y].g[1] = g2;
		vdp1_shading_data->scanline[y].r[1] = r2;

		vdp1_shading_data->scanline[y].db = gbd;
		vdp1_shading_data->scanline[y].dg = ggd;
		vdp1_shading_data->scanline[y].dr = grd;

	}
}

static void vdp1_setup_shading_for_slope(
		INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 *nx1, INT32 *nx2,
		INT32 r1, INT32 r2, INT32 slr1, INT32 slr2, INT32 *nr1, INT32 *nr2,
		INT32 g1, INT32 g2, INT32 slg1, INT32 slg2, INT32 *ng1, INT32 *ng2,
		INT32 b1, INT32 b2, INT32 slb1, INT32 slb2, INT32 *nb1, INT32 *nb2,
		INT32 _y1, INT32 y2)
{
	if(x1 > x2 || (x1==x2 && sl1 > sl2)) {
		SWAP_INT(x1, x2);
		SWAP_INT(sl1, sl2);
		SWAP_PTR(nx1, nx2);
		SWAP_INT(r1, r2);
		SWAP_INT(slr1, slr2);
		SWAP_PTR(nr1, nr2);
		SWAP_INT(g1, g2);
		SWAP_INT(slg1, slg2);
		SWAP_PTR(ng1, ng2);
		SWAP_INT(b1, b2);
		SWAP_INT(slb1, slb2);
		SWAP_PTR(nb1, nb2);
	}

	while(_y1 < y2)
	{
		vdp1_setup_shading_for_line(_y1, x1, x2, r1, g1, b1, r2, g2, b2);
		x1 += sl1;
		r1 += slr1;
		g1 += slg1;
		b1 += slb1;

		x2 += sl2;
		r2 += slr2;
		g2 += slg2;
		b2 += slb2;
		_y1++;
	}
	*nx1 = x1;
	*nr1 = r1;
	*ng1 = g1;
	*nb1 = b1;

	*nx2 = x2;
	*nr2 = r2;
	*nb2 = b2;
	*ng2 = g2;
}

static void vdp1_setup_shading(const struct spoint* q, const struct rectangle *cliprect)
{
	INT32 x1, x2, delta, cury, limy;
	INT32 r1, g1, b1, r2, g2, b2;
	INT32 sl1, slg1, slb1, slr1;
	INT32 sl2, slg2, slb2, slr2;
	int pmin, pmax, i, ps1, ps2;
	struct shaded_point p[8];
	UINT16 gd[4];

	if ( read_gouraud_table() == 0 ) return;

	gd[0] = gouraud_shading.GA;
	gd[1] = gouraud_shading.GB;
	gd[2] = gouraud_shading.GC;
	gd[3] = gouraud_shading.GD;

	for(i=0; i<4; i++) {
		p[i].x = p[i+4].x = q[i].x << FRAC_SHIFT;
		p[i].y = p[i+4].y = q[i].y;
		p[i].r = p[i+4].r = RGB_R(gd[i]) << FRAC_SHIFT;
		p[i].g = p[i+4].g = RGB_G(gd[i]) << FRAC_SHIFT;
		p[i].b = p[i+4].b = RGB_B(gd[i]) << FRAC_SHIFT;
	}

	pmin = pmax = 0;
	for(i=1; i<4; i++) {
		if(p[i].y < p[pmin].y)
			pmin = i;
		if(p[i].y > p[pmax].y)
			pmax = i;
	}

	cury = p[pmin].y;
	limy = p[pmax].y;

	vdp1_shading_data->sy = cury;
	vdp1_shading_data->ey = limy;

	if(cury == limy) {
		x1 = x2 = p[0].x;
		ps1 = ps2 = 0;
		for(i=1; i<4; i++) {
			if(p[i].x < x1) {
				x1 = p[i].x;
				ps1 = i;
			}
			if(p[i].x > x2) {
				x2 = p[i].x;
				ps2 = i;
			}
		}
		vdp1_setup_shading_for_line(cury, x1, x2, p[ps1].r, p[ps1].g, p[ps1].b, p[ps2].r, p[ps2].g, p[ps2].b);
		goto finish;
	}

	ps1 = pmin+4;
	ps2 = pmin;

	goto startup;

	for(;;) {
		if(p[ps1-1].y == p[ps2+1].y) {
			vdp1_setup_shading_for_slope(
							x1, x2, sl1, sl2, &x1, &x2,
							r1, r2, slr1, slr2, &r1, &r2,
							g1, g2, slg1, slg2, &g1, &g2,
							b1, b2, slb1, slb2, &b1, &b2,
							cury, p[ps1-1].y);
			cury = p[ps1-1].y;
			if(cury >= limy)
				break;
			ps1--;
			ps2++;

		startup:
			while(p[ps1-1].y == cury)
				ps1--;
			while(p[ps2+1].y == cury)
				ps2++;
			x1 = p[ps1].x;
			r1 = p[ps1].r;
			g1 = p[ps1].g;
			b1 = p[ps1].b;
			x2 = p[ps2].x;
			r2 = p[ps2].r;
			g2 = p[ps2].g;
			b2 = p[ps2].b;

			delta = cury-p[ps1-1].y;
			sl1 = (x1-p[ps1-1].x)/delta;
			slr1 = (r1-p[ps1-1].r)/delta;
			slg1 = (g1-p[ps1-1].g)/delta;
			slb1 = (b1-p[ps1-1].b)/delta;

			delta = cury-p[ps2+1].y;
			sl2 = (x2-p[ps2+1].x)/delta;
			slr2 = (r2-p[ps2+1].r)/delta;
			slg2 = (g2-p[ps2+1].g)/delta;
			slb2 = (b2-p[ps2+1].b)/delta;
		} else if(p[ps1-1].y < p[ps2+1].y) {
			vdp1_setup_shading_for_slope(
							x1, x2, sl1, sl2, &x1, &x2,
							r1, r2, slr1, slr2, &r1, &r2,
							g1, g2, slg1, slg2, &g1, &g2,
							b1, b2, slb1, slb2, &b1, &b2,
							cury, p[ps1-1].y);
			cury = p[ps1-1].y;
			if(cury >= limy)
				break;
			ps1--;
			while(p[ps1-1].y == cury)
				ps1--;
			x1 = p[ps1].x;
			r1 = p[ps1].r;
			g1 = p[ps1].g;
			b1 = p[ps1].b;

			delta = cury-p[ps1-1].y;
			sl1 = (x1-p[ps1-1].x)/delta;
			slr1 = (r1-p[ps1-1].r)/delta;
			slg1 = (g1-p[ps1-1].g)/delta;
			slb1 = (b1-p[ps1-1].b)/delta;
		} else {
			vdp1_setup_shading_for_slope(
							x1, x2, sl1, sl2, &x1, &x2,
							r1, r2, slr1, slr2, &r1, &r2,
							g1, g2, slg1, slg2, &g1, &g2,
							b1, b2, slb1, slb2, &b1, &b2,
							cury, p[ps2+1].y);
			cury = p[ps2+1].y;
			if(cury >= limy)
				break;
			ps2++;
			while(p[ps2+1].y == cury)
				ps2++;
			x2 = p[ps2].x;
			r2 = p[ps2].r;
			g2 = p[ps2].g;
			b2 = p[ps2].b;

			delta = cury-p[ps2+1].y;
			sl2 = (x2-p[ps2+1].x)/delta;
			slr2 = (r2-p[ps2+1].r)/delta;
			slg2 = (g2-p[ps2+1].g)/delta;
			slb2 = (b2-p[ps2+1].b)/delta;
		}
	}
	if(cury == limy)
		vdp1_setup_shading_for_line(cury, x1, x2, r1, g1, b1, r2, g2, b2 );

finish:

	if ( vdp1_shading_data->sy < 0 ) vdp1_shading_data->sy = 0;
	if ( vdp1_shading_data->sy >= 512 ) return;
	if ( vdp1_shading_data->ey < 0 ) return;
	if ( vdp1_shading_data->ey >= 512 ) vdp1_shading_data->ey = 511;

	for ( cury = vdp1_shading_data->sy; cury <= vdp1_shading_data->ey; cury++ )
	{
		while( (vdp1_shading_data->scanline[cury].x[0] >> 16) < cliprect->min_x )
		{
			vdp1_shading_data->scanline[cury].x[0] += (1 << FRAC_SHIFT);
			vdp1_shading_data->scanline[cury].b[0] += vdp1_shading_data->scanline[cury].db;
			vdp1_shading_data->scanline[cury].g[0] += vdp1_shading_data->scanline[cury].dg;
			vdp1_shading_data->scanline[cury].r[0] += vdp1_shading_data->scanline[cury].dr;
		}
	}

}

/* note that if we're drawing
to the framebuffer we CAN'T frameskip the vdp1 drawing as the hardware can READ the framebuffer
and if we skip the drawing the content could be incorrect when it reads it, although i have no idea
why they would want to */



static void drawpixel_poly(int x, int y, int patterndata, int offsetcnt)
{
	/* Capcom Collection Dai 4 uses a dummy polygon to clear VDP1 framebuffer that goes over our current max size ... */
	if(x >= 1024 || y >= 512)
		return;

	vdp1.framebuffer_draw_lines[y][x] = current_sprite.CMDCOLR;
}

static void drawpixel_8bpp_trans(int x, int y, int patterndata, int offsetcnt)
{
	UINT16 pix;

	pix = vdp1.gfx_decode[patterndata+offsetcnt] & 0xff;
	if ( pix != 0 )
	{
		vdp1.framebuffer_draw_lines[y][x] = pix | sprite_colorbank;
	}
}

static void drawpixel_4bpp_notrans(int x, int y, int patterndata, int offsetcnt)
{
	UINT16 pix;

	pix = vdp1.gfx_decode[patterndata+offsetcnt/2];
	pix = offsetcnt&1 ? (pix & 0x0f) : ((pix & 0xf0)>>4);
	vdp1.framebuffer_draw_lines[y][x] = pix | sprite_colorbank;
}

static void drawpixel_4bpp_trans(int x, int y, int patterndata, int offsetcnt)
{
	UINT16 pix;

	pix = vdp1.gfx_decode[patterndata+offsetcnt/2];
	pix = offsetcnt&1 ? (pix & 0x0f) : ((pix & 0xf0)>>4);
	if ( pix != 0 )
		vdp1.framebuffer_draw_lines[y][x] = pix | sprite_colorbank;
}

static void drawpixel_generic(int x, int y, int patterndata, int offsetcnt)
{
	int pix,transpen, spd = current_sprite.CMDPMOD & 0x40;
/*  int mode; */
	int mesh = current_sprite.CMDPMOD & 0x100;
	int raw,endcode;

	if ( mesh && !((x ^ y) & 1) )
	{
		return;
	}

	if(x >= 1024 || y >= 512)
		return;

	if ( current_sprite.ispoly )
	{
		raw = pix = current_sprite.CMDCOLR&0xffff;

		transpen = 0;
		endcode = 0xffff;
		#if 0
		if ( pix & 0x8000 )
		{
			mode = 5;
		}
		else
		{
			mode = 1;
		}
		#endif
	}
	else
	{
		switch (current_sprite.CMDPMOD&0x0038)
		{
			case 0x0000: /* mode 0 16 colour bank mode (4bits) (hanagumi blocks) */
				/* most of the shienryu sprites use this mode */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt/2) & 0xfffff];
				raw = offsetcnt&1 ? (raw & 0x0f) : ((raw & 0xf0)>>4);
				pix = raw+((current_sprite.CMDCOLR&0xfff0));
				/*mode = 0; */
				transpen = 0;
				endcode = 0xf;
				break;
			case 0x0008: /* mode 1 16 colour lookup table mode (4bits) */
				/* shienryu explosions (and some enemies) use this mode */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt/2) & 0xfffff];
				raw = offsetcnt&1 ? (raw & 0x0f) : ((raw & 0xf0)>>4);
				pix = raw&1 ?
				((((vdp1_vram[(((current_sprite.CMDCOLR&0xffff)*8)>>2)+((raw&0xfffe)/2)])) & 0x0000ffff) >> 0):
				((((vdp1_vram[(((current_sprite.CMDCOLR&0xffff)*8)>>2)+((raw&0xfffe)/2)])) & 0xffff0000) >> 16);
				/*mode = 5; */
				transpen = 0;
				endcode = 0xf;
				break;
			case 0x0010: /* mode 2 64 colour bank mode (8bits) (character select portraits on hanagumi) */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt) & 0xfffff] & 0xff;
				/*mode = 2; */
				pix = raw+(current_sprite.CMDCOLR&0xffc0);
				transpen = 0;
				endcode = 0xff;
				/* Notes of interest: */
				/* Scud: the disposable assassin wants transparent pen on 0 */
				/* sasissu: racing stage background clouds */
				break;
			case 0x0018: /* mode 3 128 colour bank mode (8bits) (little characters on hanagumi use this mode) */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt) & 0xfffff] & 0xff;
				pix = raw+(current_sprite.CMDCOLR&0xff80);
				transpen = 0;
				endcode = 0xff;
				/*mode = 3; */
				break;
			case 0x0020: /* mode 4 256 colour bank mode (8bits) (hanagumi title) */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt) & 0xfffff] & 0xff;
				pix = raw+(current_sprite.CMDCOLR&0xff00);
				transpen = 0;
				endcode = 0xff;
				/*mode = 4; */
				break;
			case 0x0028: /* mode 5 32,768 colour RGB mode (16bits) */
				raw = vdp1.gfx_decode[(patterndata+offsetcnt*2+1) & 0xfffff] | (vdp1.gfx_decode[(patterndata+offsetcnt*2) & 0xfffff]<<8);
				/*mode = 5; */
				/* TODO: 0x1-0x7ffe reserved (color bank) */
				pix = raw;
				transpen = 0;
				endcode = 0x7fff;
				break;
			case 0x0038: /* invalid */
				/* game tengoku uses this on hi score screen (tate mode) */
				/* according to Charles, reads from VRAM address 0 */
				raw = pix = vdp1.gfx_decode[1] | (vdp1.gfx_decode[0]<<8) ;
				/* TODO: check transpen */
				transpen = 0;
				endcode = -1;
				break;
			default: /* other settings illegal */
				pix = mame_rand_video();
				raw = pix & 0xff; /* just mimic old driver behavior */
				/*mode = 0; */
				transpen = 0;
				endcode = 0xff;
				popmessage("Illegal Sprite Mode %02x",current_sprite.CMDPMOD&0x0038);
		}


		/* preliminary end code disable support */
		if ( ((current_sprite.CMDPMOD & 0x80) == 0) &&
			(raw == endcode) )
		{
			return;
		}
	}

	/* MSBON */
	/* TODO: does this always applies to the frame buffer regardless of the mode? */
	pix |= current_sprite.CMDPMOD & 0x8000;
	/*
	TODO: from docs:
	"Except for the color calculation of replace and shadow, color calculation can only be performed when the color code of the original picture is RGB code.
	Color calculation can be executed when the color code is color bank code, but the results are not guaranteed."
	Currently no idea about the "result not guaranteed" part, let's disable this branch for the time being ...
	*/
	#if 0
	if ( mode != 5 )
	{
		if ( (raw != transpen) || spd )
		{
			vdp1.framebuffer_draw_lines[y][x] = pix;
		}
	}
	else
	#endif
	{
		if ( (raw != transpen) || spd )
		{
			if ( current_sprite.CMDPMOD & 0x4 ) /* Gouraud shading */
				pix = vdp1_apply_gouraud_shading( x, y, pix );

			switch( current_sprite.CMDPMOD & 0x3 )
			{
				case 0: /* replace */
					vdp1.framebuffer_draw_lines[y][x] = pix;
					break;
				case 1: /* shadow */
					if ( vdp1.framebuffer_draw_lines[y][x] & 0x8000 )
					{
						vdp1.framebuffer_draw_lines[y][x] = ((vdp1.framebuffer_draw_lines[y][x] & ~0x8421) >> 1) | 0x8000;
					}
					break;
				case 2: /* half luminance */
					vdp1.framebuffer_draw_lines[y][x] = ((pix & ~0x8421) >> 1) | 0x8000;
					break;
				case 3: /* half transparent */
					if ( vdp1.framebuffer_draw_lines[y][x] & 0x8000 )
					{
						vdp1.framebuffer_draw_lines[y][x] = stv_alpha_blend_r16( vdp1.framebuffer_draw_lines[y][x], pix, 0x80 ) | 0x8000;
					}
					else
					{
						vdp1.framebuffer_draw_lines[y][x] = pix;
					}
					break;
				/*case 4:  Gouraud shading */
				/* TODO: proyakts (during team creation, on PR girl select) */
				/*case 6: */
				/*  break; */
				/*case 7:  Gouraud-shading + half-transparent */
					/* lupinpy enemy shadows */
					/* deathcri lives indicators */
					/* TODO: latter looks really bad. */
				default:
					/* TODO: mode 5: prohibited, mode 6: gouraud shading + half-luminance, mode 7: gouraud-shading + half-transparent */
					popmessage("VDP1 PMOD = %02x",current_sprite.CMDPMOD & 0x7);
					vdp1.framebuffer_draw_lines[y][x] = pix;
					break;
			}
		}
	}
}


static void vdp1_set_drawpixel(void)
{
	int sprite_type = current_sprite.CMDCTRL & 0x000f;
	int sprite_mode = current_sprite.CMDPMOD&0x0038;
	int spd = current_sprite.CMDPMOD & 0x40;
	int mesh = current_sprite.CMDPMOD & 0x100;
	int ecd = current_sprite.CMDPMOD & 0x80;

	if ( mesh || !ecd || ((current_sprite.CMDPMOD & 0x7) != 0) )
	{
		drawpixel = drawpixel_generic;
		return;
	}

	if(current_sprite.CMDPMOD & 0x8000)
	{
		drawpixel = drawpixel_generic;
		return;
	}

	/* polygon / polyline / line with replace case */
	if (sprite_type & 4 && ((current_sprite.CMDPMOD & 0x7) == 0))
	{
		drawpixel = drawpixel_poly;
	}
	else if ( (sprite_mode == 0x20) && !spd )
	{
		sprite_colorbank = (current_sprite.CMDCOLR&0xff00);
		drawpixel = drawpixel_8bpp_trans;
	}
	else if ((sprite_mode == 0x00) && spd)
	{
		sprite_colorbank = (current_sprite.CMDCOLR&0xfff0);
		drawpixel = drawpixel_4bpp_notrans;
	}
	else if (sprite_mode == 0x00 && !spd )
	{
		sprite_colorbank = (current_sprite.CMDCOLR&0xfff0);
		drawpixel = drawpixel_4bpp_trans;
	}
	else
	{
		drawpixel = drawpixel_generic;
	}
}


static void vdp1_fill_slope(const struct rectangle *cliprect, int patterndata, int xsize,
							INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 *nx1, INT32 *nx2,
							INT32 u1, INT32 u2, INT32 slu1, INT32 slu2, INT32 *nu1, INT32 *nu2,
							INT32 v1, INT32 v2, INT32 slv1, INT32 slv2, INT32 *nv1, INT32 *nv2,
							INT32 _y1, INT32 y2)
{
	if(_y1 > cliprect->max_y)
		return;

	if(y2 <= cliprect->min_y) {
		int delta = y2-_y1;
		*nx1 = x1+delta*sl1;
		*nu1 = u1+delta*slu1;
		*nv1 = v1+delta*slv1;
		*nx2 = x2+delta*sl2;
		*nu2 = u2+delta*slu2;
		*nv2 = v2+delta*slv2;
		return;
	}

	if(y2 > cliprect->max_y)
		y2 = cliprect->max_y+1;

	if(_y1 < cliprect->min_y) {
		int delta = cliprect->min_y - _y1;
		x1 += delta*sl1;
		u1 += delta*slu1;
		v1 += delta*slv1;
		x2 += delta*sl2;
		u2 += delta*slu2;
		v2 += delta*slv2;
		_y1 = cliprect->min_y;
	}

	if(x1 > x2 || (x1==x2 && sl1 > sl2)) {
		INT32 t, *tp;
		t = x1;
		x1 = x2;
		x2 = t;
		t = sl1;
		sl1 = sl2;
		sl2 = t;
		tp = nx1;
		nx1 = nx2;
		nx2 = tp;

		t = u1;
		u1 = u2;
		u2 = t;
		t = slu1;
		slu1 = slu2;
		slu2 = t;
		tp = nu1;
		nu1 = nu2;
		nu2 = tp;

		t = v1;
		v1 = v2;
		v2 = t;
		t = slv1;
		slv1 = slv2;
		slv2 = t;
		tp = nv1;
		nv1 = nv2;
		nv2 = tp;
	}

	while(_y1 < y2) {
		if(_y1 >= cliprect->min_y) {
			INT32 slux = 0, slvx = 0;
			int xx1 = x1>>FRAC_SHIFT;
			int xx2 = x2>>FRAC_SHIFT;
			INT32 u = u1;
			INT32 v = v1;
			if(xx1 != xx2) {
				int delta = xx2-xx1;
				slux = (u2-u1)/delta;
				slvx = (v2-v1)/delta;
			}
			if(xx1 <= cliprect->max_x || xx2 >= cliprect->min_x) {
				if(xx1 < cliprect->min_x) {
					int delta = cliprect->min_x-xx1;
					u += slux*delta;
					v += slvx*delta;
					xx1 = cliprect->min_x;
				}
				if(xx2 > cliprect->max_x)
					xx2 = cliprect->max_x;

				while(xx1 <= xx2) {
					drawpixel(xx1,_y1, patterndata, (v>>FRAC_SHIFT)*xsize+(u>>FRAC_SHIFT));
					xx1++;
					u += slux;
					v += slvx;
				}
			}
		}

		x1 += sl1;
		u1 += slu1;
		v1 += slv1;
		x2 += sl2;
		u2 += slu2;
		v2 += slv2;
		_y1++;
	}
	*nx1 = x1;
	*nu1 = u1;
	*nv1 = v1;
	*nx2 = x2;
	*nu2 = u2;
	*nv2 = v2;
}

static void vdp1_fill_line(const struct rectangle *cliprect, int patterndata, int xsize, INT32 y,
							INT32 x1, INT32 x2, INT32 u1, INT32 u2, INT32 v1, INT32 v2)
{
	int xx1 = x1>>FRAC_SHIFT;
	int xx2 = x2>>FRAC_SHIFT;

	if(y < cliprect->min_y || y > cliprect->max_y)
		return;

	if(xx1 <= cliprect->max_x || xx2 >= cliprect->min_x) {
		INT32 slux = 0, slvx = 0;
		INT32 u = u1;
		INT32 v = v1;
		if(xx1 != xx2) {
			int delta = xx2-xx1;
			slux = (u2-u1)/delta;
			slvx = (v2-v1)/delta;
		}
		if(xx1 < cliprect->min_x) {
			int delta = cliprect->min_x-xx1;
			u += slux*delta;
			v += slvx*delta;
			xx1 = cliprect->min_x;
		}
		if(xx2 > cliprect->max_x)
			xx2 = cliprect->max_x;

		while(xx1 <= xx2) {
			drawpixel(xx1,y,patterndata,(v>>FRAC_SHIFT)*xsize+(u>>FRAC_SHIFT));
			xx1++;
			u += slux;
			v += slvx;
		}
	}
}

static void vdp1_fill_quad(const struct rectangle *cliprect, int patterndata, int xsize, const struct spoint *q)
{
	INT32 sl1, sl2, slu1, slu2, slv1, slv2, cury, limy, x1, x2, u1, u2, v1, v2, delta;
	int pmin, pmax, i, ps1, ps2;
	struct spoint p[8];

	for(i=0; i<4; i++) {
		p[i].x = p[i+4].x = q[i].x << FRAC_SHIFT;
		p[i].y = p[i+4].y = q[i].y;
		p[i].u = p[i+4].u = q[i].u << FRAC_SHIFT;
		p[i].v = p[i+4].v = q[i].v << FRAC_SHIFT;
	}

	pmin = pmax = 0;
	for(i=1; i<4; i++) {
		if(p[i].y < p[pmin].y)
			pmin = i;
		if(p[i].y > p[pmax].y)
			pmax = i;
	}

	cury = p[pmin].y;
	limy = p[pmax].y;

	if(cury == limy) {
		x1 = x2 = p[0].x;
		u1 = u2 = p[0].u;
		v1 = v2 = p[0].v;
		for(i=1; i<4; i++) {
			if(p[i].x < x1) {
				x1 = p[i].x;
				u1 = p[i].u;
				v1 = p[i].v;
			}
			if(p[i].x > x2) {
				x2 = p[i].x;
				u2 = p[i].u;
				v2 = p[i].v;
			}
		}
		vdp1_fill_line(cliprect, patterndata, xsize, cury, x1, x2, u1, u2, v1, v2);
		return;
	}

	if(cury > cliprect->max_y)
		return;
	if(limy <= cliprect->min_y)
		return;

	if(limy > cliprect->max_y)
		limy = cliprect->max_y;

	ps1 = pmin+4;
	ps2 = pmin;

	goto startup;

	for(;;) {
		if(p[ps1-1].y == p[ps2+1].y) {
			vdp1_fill_slope(cliprect, patterndata, xsize,
							x1, x2, sl1, sl2, &x1, &x2,
							u1, u2, slu1, slu2, &u1, &u2,
							v1, v2, slv1, slv2, &v1, &v2,
							cury, p[ps1-1].y);
			cury = p[ps1-1].y;
			if(cury >= limy)
				break;
			ps1--;
			ps2++;

		startup:
			while(p[ps1-1].y == cury)
				ps1--;
			while(p[ps2+1].y == cury)
				ps2++;
			x1 = p[ps1].x;
			u1 = p[ps1].u;
			v1 = p[ps1].v;
			x2 = p[ps2].x;
			u2 = p[ps2].u;
			v2 = p[ps2].v;

			delta = cury-p[ps1-1].y;
			sl1 = (x1-p[ps1-1].x)/delta;
			slu1 = (u1-p[ps1-1].u)/delta;
			slv1 = (v1-p[ps1-1].v)/delta;

			delta = cury-p[ps2+1].y;
			sl2 = (x2-p[ps2+1].x)/delta;
			slu2 = (u2-p[ps2+1].u)/delta;
			slv2 = (v2-p[ps2+1].v)/delta;
		} else if(p[ps1-1].y < p[ps2+1].y) {
			vdp1_fill_slope(cliprect, patterndata, xsize,
							x1, x2, sl1, sl2, &x1, &x2,
							u1, u2, slu1, slu2, &u1, &u2,
							v1, v2, slv1, slv2, &v1, &v2,
							cury, p[ps1-1].y);
			cury = p[ps1-1].y;
			if(cury >= limy)
				break;
			ps1--;
			while(p[ps1-1].y == cury)
				ps1--;
			x1 = p[ps1].x;
			u1 = p[ps1].u;
			v1 = p[ps1].v;

			delta = cury-p[ps1-1].y;
			sl1 = (x1-p[ps1-1].x)/delta;
			slu1 = (u1-p[ps1-1].u)/delta;
			slv1 = (v1-p[ps1-1].v)/delta;
		} else {
			vdp1_fill_slope(cliprect, patterndata, xsize,
							x1, x2, sl1, sl2, &x1, &x2,
							u1, u2, slu1, slu2, &u1, &u2,
							v1, v2, slv1, slv2, &v1, &v2,
							cury, p[ps2+1].y);
			cury = p[ps2+1].y;
			if(cury >= limy)
				break;
			ps2++;
			while(p[ps2+1].y == cury)
				ps2++;
			x2 = p[ps2].x;
			u2 = p[ps2].u;
			v2 = p[ps2].v;

			delta = cury-p[ps2+1].y;
			sl2 = (x2-p[ps2+1].x)/delta;
			slu2 = (u2-p[ps2+1].u)/delta;
			slv2 = (v2-p[ps2+1].v)/delta;
		}
	}
	if(cury == limy)
		vdp1_fill_line(cliprect, patterndata, xsize, cury, x1, x2, u1, u2, v1, v2);
}

static int x2s(int v)
{
	return (INT32)(INT16)v + vdp1.local_x;
}

static int y2s(int v)
{
	return (INT32)(INT16)v + vdp1.local_y;
}

static void vdp1_draw_line(const struct rectangle *cliprect)
{
	struct spoint q[4];

	q[0].x = x2s(current_sprite.CMDXA);
	q[0].y = y2s(current_sprite.CMDYA);
	q[1].x = x2s(current_sprite.CMDXB);
	q[1].y = y2s(current_sprite.CMDYB);
	q[2].x = x2s(current_sprite.CMDXA);
	q[2].y = y2s(current_sprite.CMDYA);
	q[3].x = x2s(current_sprite.CMDXB);
	q[3].y = y2s(current_sprite.CMDYB);

	q[0].u = q[3].u = q[1].u = q[2].u = 0;
	q[0].v = q[1].v = q[2].v = q[3].v = 0;

	vdp1_fill_quad(cliprect, 0, 1, q);
}

static void vdp1_draw_poly_line(const struct rectangle *cliprect)
{
	struct spoint q[4];

	q[0].x = x2s(current_sprite.CMDXA);
	q[0].y = y2s(current_sprite.CMDYA);
	q[1].x = x2s(current_sprite.CMDXB);
	q[1].y = y2s(current_sprite.CMDYB);
	q[2].x = x2s(current_sprite.CMDXA);
	q[2].y = y2s(current_sprite.CMDYA);
	q[3].x = x2s(current_sprite.CMDXB);
	q[3].y = y2s(current_sprite.CMDYB);

	q[0].u = q[3].u = q[1].u = q[2].u = 0;
	q[0].v = q[1].v = q[2].v = q[3].v = 0;

	vdp1_fill_quad(cliprect, 0, 1, q);

	q[0].x = x2s(current_sprite.CMDXB);
	q[0].y = y2s(current_sprite.CMDYB);
	q[1].x = x2s(current_sprite.CMDXC);
	q[1].y = y2s(current_sprite.CMDYC);
	q[2].x = x2s(current_sprite.CMDXB);
	q[2].y = y2s(current_sprite.CMDYB);
	q[3].x = x2s(current_sprite.CMDXC);
	q[3].y = y2s(current_sprite.CMDYC);

	q[0].u = q[3].u = q[1].u = q[2].u = 0;
	q[0].v = q[1].v = q[2].v = q[3].v = 0;

	vdp1_fill_quad(cliprect, 0, 1, q);

	q[0].x = x2s(current_sprite.CMDXC);
	q[0].y = y2s(current_sprite.CMDYC);
	q[1].x = x2s(current_sprite.CMDXD);
	q[1].y = y2s(current_sprite.CMDYD);
	q[2].x = x2s(current_sprite.CMDXC);
	q[2].y = y2s(current_sprite.CMDYC);
	q[3].x = x2s(current_sprite.CMDXD);
	q[3].y = y2s(current_sprite.CMDYD);

	q[0].u = q[3].u = q[1].u = q[2].u = 0;
	q[0].v = q[1].v = q[2].v = q[3].v = 0;

	vdp1_fill_quad(cliprect, 0, 1, q);

	q[0].x = x2s(current_sprite.CMDXD);
	q[0].y = y2s(current_sprite.CMDYD);
	q[1].x = x2s(current_sprite.CMDXA);
	q[1].y = y2s(current_sprite.CMDYA);
	q[2].x = x2s(current_sprite.CMDXD);
	q[2].y = y2s(current_sprite.CMDYD);
	q[3].x = x2s(current_sprite.CMDXA);
	q[3].y = y2s(current_sprite.CMDYA);

	q[0].u = q[3].u = q[1].u = q[2].u = 0;
	q[0].v = q[1].v = q[2].v = q[3].v = 0;

	vdp1_setup_shading(q, cliprect);
	vdp1_fill_quad(cliprect, 0, 1, q);

}

static void vdp1_draw_distorted_sprite(const struct rectangle *cliprect)
{
	struct spoint q[4];

	int xsize, ysize;
	int direction;
	int patterndata;

	direction = (current_sprite.CMDCTRL & 0x0030)>>4;

	if ( current_sprite.ispoly )
	{
		xsize = ysize = 1;
		patterndata = 0;
	}
	else
	{
		xsize = (current_sprite.CMDSIZE & 0x3f00) >> 8;
		xsize = xsize * 8;
		if (xsize == 0) return; /* setting prohibited */

		ysize = (current_sprite.CMDSIZE & 0x00ff);
		if (ysize == 0) return; /* setting prohibited */

		patterndata = (current_sprite.CMDSRCA) & 0xffff;
		patterndata = patterndata * 0x8;

	}


	q[0].x = x2s(current_sprite.CMDXA);
	q[0].y = y2s(current_sprite.CMDYA);
	q[1].x = x2s(current_sprite.CMDXB);
	q[1].y = y2s(current_sprite.CMDYB);
	q[2].x = x2s(current_sprite.CMDXC);
	q[2].y = y2s(current_sprite.CMDYC);
	q[3].x = x2s(current_sprite.CMDXD);
	q[3].y = y2s(current_sprite.CMDYD);

	if(direction & 1) { /* xflip */
		q[0].u = q[3].u = xsize-1;
		q[1].u = q[2].u = 0;
	} else {
		q[0].u = q[3].u = 0;
		q[1].u = q[2].u = xsize-1;
	}
	if(direction & 2) { /* yflip */
		q[0].v = q[1].v = ysize-1;
		q[2].v = q[3].v = 0;
	} else {
		q[0].v = q[1].v = 0;
		q[2].v = q[3].v = ysize-1;
	}

	vdp1_setup_shading(q, cliprect);
	vdp1_fill_quad(cliprect, patterndata, xsize, q);
}

static void vdp1_draw_scaled_sprite(const struct rectangle *cliprect)
{
	struct spoint q[4];

	int xsize, ysize;
	int direction;
	int patterndata;
	int zoompoint;
	int x,y;
	int x2,y2;
	int screen_width,screen_height,screen_height_negative = 0;

	direction = (current_sprite.CMDCTRL & 0x0030)>>4;

	xsize = (current_sprite.CMDSIZE & 0x3f00) >> 8;
	xsize = xsize * 8;

	ysize = (current_sprite.CMDSIZE & 0x00ff);

	patterndata = (current_sprite.CMDSRCA) & 0xffff;
	patterndata = patterndata * 0x8;

	zoompoint = (current_sprite.CMDCTRL & 0x0f00)>>8;

	x = current_sprite.CMDXA;
	y = current_sprite.CMDYA;

	screen_width = (INT16)current_sprite.CMDXB;
	if ( (screen_width < 0) && zoompoint)
	{
		screen_width = -screen_width;
		direction |= 1;
	}

	screen_height = (INT16)current_sprite.CMDYB;
	if ( (screen_height < 0) && zoompoint )
	{
		screen_height_negative = 1;
		screen_height = -screen_height;
		direction |= 2;
	}

	x2 = current_sprite.CMDXC; /* second co-ordinate set x */
	y2 = current_sprite.CMDYC; /* second co-ordinate set y */

	switch (zoompoint)
	{
		case 0x0: /* specified co-ordinates */
			break;
		case 0x5: /* up left */
			break;
		case 0x6: /* up center */
			x -= screen_width/2 ;
			break;
		case 0x7: /* up right */
			x -= screen_width;
			break;

		case 0x9: /* center left */
			y -= screen_height/2 ;
			break;
		case 0xa: /* center center */
			y -= screen_height/2 ;
			x -= screen_width/2 ;

			break;

		case 0xb: /* center right */
			y -= screen_height/2 ;
			x -= screen_width;
			break;

		case 0xd: /* center left */
			y -= screen_height;
			break;

		case 0xe: /* center center */
			y -= screen_height;
			x -= screen_width/2 ;
			break;

		case 0xf: /* center right */
			y -= screen_height;
			x -= screen_width;
			break;

		default: /* illegal */
			break;

	}

	/*  0----1
	    |    |
	    |    |
	    3----2   */

	if (zoompoint)
	{
		q[0].x = x2s(x);
		q[0].y = y2s(y);
		q[1].x = x2s(x)+screen_width;
		q[1].y = y2s(y);
		q[2].x = x2s(x)+screen_width;
		q[2].y = y2s(y)+screen_height;
		q[3].x = x2s(x);
		q[3].y = y2s(y)+screen_height;

		if ( screen_height_negative )
		{
			q[0].y += screen_height;
			q[1].y += screen_height;
			q[2].y += screen_height;
			q[3].y += screen_height;
		}
	}
	else
	{
		q[0].x = x2s(x);
		q[0].y = y2s(y);
		q[1].x = x2s(x2);
		q[1].y = y2s(y);
		q[2].x = x2s(x2);
		q[2].y = y2s(y2);
		q[3].x = x2s(x);
		q[3].y = y2s(y2);
	}


	if(direction & 1) { /* xflip */
		q[0].u = q[3].u = xsize-1;
		q[1].u = q[2].u = 0;
	} else {
		q[0].u = q[3].u = 0;
		q[1].u = q[2].u = xsize-1;
	}
	if(direction & 2) { /* yflip */
		q[0].v = q[1].v = ysize-1;
		q[2].v = q[3].v = 0;
	} else {
		q[0].v = q[1].v = 0;
		q[2].v = q[3].v = ysize-1;
	}

	vdp1_setup_shading(q, cliprect);
	vdp1_fill_quad(cliprect, patterndata, xsize, q);
}




static void vdp1_draw_normal_sprite(const struct rectangle *cliprect, int sprite_type)
{
	int y, ysize, drawypos;
	int x, xsize, drawxpos;
	int direction;
	int patterndata;
	UINT8 shading;
	int su, u, dux, duy;
	int maxdrawypos, maxdrawxpos;

	x = x2s(current_sprite.CMDXA);
	y = y2s(current_sprite.CMDYA);

	direction = (current_sprite.CMDCTRL & 0x0030)>>4;

	xsize = (current_sprite.CMDSIZE & 0x3f00) >> 8;
	xsize = xsize * 8;

	ysize = (current_sprite.CMDSIZE & 0x00ff);

	patterndata = (current_sprite.CMDSRCA) & 0xffff;
	patterndata = patterndata * 0x8;

	if (VDP1_LOG) logerror ("Drawing Normal Sprite x %04x y %04x xsize %04x ysize %04x patterndata %06x\n",x,y,xsize,ysize,patterndata);

	if ( x > cliprect->max_x ) return;
	if ( y > cliprect->max_y ) return;

	shading = read_gouraud_table();
	if ( shading )
	{
		struct spoint q[4];
		q[0].x = x; q[0].y = y;
		q[1].x = x + xsize; q[1].y = y;
		q[2].x = x + xsize; q[2].y = y + ysize;
		q[3].x = x; q[3].y = y + ysize;

		vdp1_setup_shading( q, cliprect );
	}

	u = 0;
	dux = 1;
	duy = xsize;
	if ( direction & 0x1 ) /*xflip */
	{
		dux = -1;
		u = xsize - 1;
	}
	if ( direction & 0x2 ) /*yflip */
	{
		duy = -xsize;
		u += xsize*(ysize-1);
	}
	if ( y < cliprect->min_y ) /*clip y */
	{
		/* draculax user clips a 320x240 sprite for inverted castle map (obviously x & y flipped) */
		/* we need to adjust U calculation only to make it align properly, */
		/* adjusting ysize will already glitch out flipped doors in gameplay. */
		const int adjust_y = (direction & 2) ? (y - cliprect->min_y) : (cliprect->min_y - y);
		u += xsize * (adjust_y);
		ysize -= (cliprect->min_y - y);
		y = cliprect->min_y;
	}
	if ( x < cliprect->min_x ) /*clip x */
	{
		u += dux*(cliprect->min_x - x);
		xsize -= (cliprect->min_x - x);
		x = cliprect->min_x;
	}
	maxdrawypos = MIN(y + ysize - 1, cliprect->max_y);
	maxdrawxpos = MIN(x + xsize - 1, cliprect->max_x);
	for (drawypos = y; drawypos <= maxdrawypos; drawypos++ )
	{
		/*destline = vdp1.framebuffer_draw_lines[drawypos]; */
		su = u;
		for (drawxpos = x; drawxpos <= maxdrawxpos; drawxpos++ )
		{
			drawpixel( drawxpos, drawypos, patterndata, u );
			u += dux;
		}
		u = su + duy;
	}
}

static void vdp1_draw_end(int param)
{
	/* set CEF to 1*/
	CEF_1();

	/* TODO: temporary for Batman Forever, presumably anonymous timer not behaving well. */
	#if 0
	if(!(m_scu.ism & IRQ_VDP1_END))
	{
		m_maincpu->set_input_line_and_vector(0x2, HOLD_LINE, 0x4d); /* SH2 */
		scu_do_transfer(6);
	}
	else
		m_scu.ist |= (IRQ_VDP1_END);
	#endif
}


static void vdp1_process_list(void)
{
	int position;
	int spritecount;
	int vdp1_nest;
	struct rectangle *cliprect;

	spritecount = 0;
	position = 0;

	if (VDP1_LOG) logerror ("Sprite List Process START\n");

	vdp1_nest = -1;

	clear_gouraud_shading();

	/*Set CEF bit to 0*/
	CEF_0();

	/* TODO: is there an actual limit for this? */
	while (spritecount < 16383) /* max 16383 with texture or max 16384 without texture - virtually unlimited */
	{
		int draw_this_sprite;

		draw_this_sprite = 1;

	/*  if (position >= ((0x80000/0x20)/4)) // safety check */
	/*  { */
	/*      if (VDP1_LOG) logerror ("Sprite List Position Too High!\n"); */
	/*      position = 0; */
	/*  } */

		spritecount++;

		current_sprite.CMDCTRL = (vdp1_vram[position * (0x20/4)+0] & 0xffff0000) >> 16;

		/* The END bit terminates the list regardless of the other bits. */
		/* (SGL terminates its sorted list with 0xffff) */
		if (current_sprite.CMDCTRL & 0x8000)
		{
			if (VDP1_LOG) logerror ("List Terminator (0x8000) Encountered, Sprite List Process END\n");
			goto end; /* end of list */
		}

		current_sprite.CMDLINK = (vdp1_vram[position * (0x20/4)+0] & 0x0000ffff) >> 0;
		current_sprite.CMDPMOD = (vdp1_vram[position * (0x20/4)+1] & 0xffff0000) >> 16;
		current_sprite.CMDCOLR = (vdp1_vram[position * (0x20/4)+1] & 0x0000ffff) >> 0;
		current_sprite.CMDSRCA = (vdp1_vram[position * (0x20/4)+2] & 0xffff0000) >> 16;
		current_sprite.CMDSIZE = (vdp1_vram[position * (0x20/4)+2] & 0x0000ffff) >> 0;
		current_sprite.CMDXA   = (vdp1_vram[position * (0x20/4)+3] & 0xffff0000) >> 16;
		current_sprite.CMDYA   = (vdp1_vram[position * (0x20/4)+3] & 0x0000ffff) >> 0;
		current_sprite.CMDXB   = (vdp1_vram[position * (0x20/4)+4] & 0xffff0000) >> 16;
		current_sprite.CMDYB   = (vdp1_vram[position * (0x20/4)+4] & 0x0000ffff) >> 0;
		current_sprite.CMDXC   = (vdp1_vram[position * (0x20/4)+5] & 0xffff0000) >> 16;
		current_sprite.CMDYC   = (vdp1_vram[position * (0x20/4)+5] & 0x0000ffff) >> 0;
		current_sprite.CMDXD   = (vdp1_vram[position * (0x20/4)+6] & 0xffff0000) >> 16;
		current_sprite.CMDYD   = (vdp1_vram[position * (0x20/4)+6] & 0x0000ffff) >> 0;
		current_sprite.CMDGRDA = (vdp1_vram[position * (0x20/4)+7] & 0xffff0000) >> 16;
/*      current_sprite.UNUSED  = (vdp1_vram[position * (0x20/4)+7] & 0x0000ffff) >> 0; */

		/* proecess jump / skip commands, set position for next sprite */
		switch (current_sprite.CMDCTRL & 0x7000)
		{
			case 0x0000: /* jump next */
				if (VDP1_LOG) logerror ("Sprite List Process + Next (Normal)\n");
				position++;
				break;
			case 0x1000: /* jump assign */
				if (VDP1_LOG) logerror ("Sprite List Process + Jump Old %06x New %06x\n", position, (current_sprite.CMDLINK>>2));
				position= (current_sprite.CMDLINK>>2);
				break;
			case 0x2000: /* jump call */
				if (vdp1_nest == -1)
				{
					if (VDP1_LOG) logerror ("Sprite List Process + Call Old %06x New %06x\n",position, (current_sprite.CMDLINK>>2));
					vdp1_nest = position+1;
					position = (current_sprite.CMDLINK>>2);
				}
				else
				{
					if (VDP1_LOG) logerror ("Sprite List Nested Call, ignoring\n");
					position++;
				}
				break;
			case 0x3000:
				if (vdp1_nest != -1)
				{
					if (VDP1_LOG) logerror ("Sprite List Process + Return\n");
					position = vdp1_nest;
					vdp1_nest = -1;
				}
				else
				{
					/* A return with no subroutine is a NOP (Yabause and Mednafen agree) */
					if (VDP1_LOG) logerror ("Attempted return from no subroutine, continuing\n");
					position++;
				}
				break;
			case 0x4000:
				draw_this_sprite = 0;
				position++;
				break;
			case 0x5000:
				if (VDP1_LOG) logerror ("Sprite List Skip + Jump Old %06x New %06x\n", position, (current_sprite.CMDLINK>>2));
				draw_this_sprite = 0;
				position= (current_sprite.CMDLINK>>2);

				break;
			case 0x6000:
				draw_this_sprite = 0;
				if (vdp1_nest == -1)
				{
					if (VDP1_LOG) logerror ("Sprite List Skip + Call To Subroutine Old %06x New %06x\n",position, (current_sprite.CMDLINK>>2));

					vdp1_nest = position+1;
					position = (current_sprite.CMDLINK>>2);
				}
				else
				{
					if (VDP1_LOG) logerror ("Sprite List Nested Call, ignoring\n");
					position++;
				}
				break;
			case 0x7000:
				draw_this_sprite = 0;
				if (vdp1_nest != -1)
				{
					if (VDP1_LOG) logerror ("Sprite List Skip + Return from Subroutine\n");

					position = vdp1_nest;
					vdp1_nest = -1;
				}
				else
				{
					if (VDP1_LOG) logerror ("Attempted return from no subroutine, continuing\n");
					position++;
				}
				break;
		}

		/* continue to draw this sprite only if the command wasn't to skip it */
		if (draw_this_sprite == 1)
		{
			if ( current_sprite.CMDPMOD & 0x0400 )
			{
				/*if(current_sprite.CMDPMOD & 0x0200)  TODO: Bio Hazard inventory screen uses outside cliprect */
				/*  cliprect = &vdp1.system_cliprect; */
				/*else */
					cliprect = &vdp1.user_cliprect;
			}
			else
			{
				cliprect = &vdp1.system_cliprect;
			}

			vdp1_set_drawpixel();

			switch (current_sprite.CMDCTRL & 0x000f)
			{
				case 0x0000:
					if (VDP1_LOG) logerror ("Sprite List Normal Sprite (%d %d)\n",current_sprite.CMDXA,current_sprite.CMDYA);
					current_sprite.ispoly = 0;
					vdp1_draw_normal_sprite(cliprect, 0);
					break;

				case 0x0001:
					if (VDP1_LOG) logerror ("Sprite List Scaled Sprite (%d %d)\n",current_sprite.CMDXA,current_sprite.CMDYA);
					current_sprite.ispoly = 0;
					vdp1_draw_scaled_sprite(cliprect);
					break;

				case 0x0002:
				case 0x0003: /* used by Hardcore 4x4 */
					if (VDP1_LOG) logerror ("Sprite List Distorted Sprite\n");
					if (VDP1_LOG) logerror ("(A: %d %d)\n",current_sprite.CMDXA,current_sprite.CMDYA);
					if (VDP1_LOG) logerror ("(B: %d %d)\n",current_sprite.CMDXB,current_sprite.CMDYB);
					if (VDP1_LOG) logerror ("(C: %d %d)\n",current_sprite.CMDXC,current_sprite.CMDYC);
					if (VDP1_LOG) logerror ("(D: %d %d)\n",current_sprite.CMDXD,current_sprite.CMDYD);
					if (VDP1_LOG) logerror ("CMDPMOD = %04x\n",current_sprite.CMDPMOD);

					current_sprite.ispoly = 0;
					vdp1_draw_distorted_sprite(cliprect);
					break;

				case 0x0004:
					if (VDP1_LOG) logerror ("Sprite List Polygon\n");
					current_sprite.ispoly = 1;
					vdp1_draw_distorted_sprite(cliprect);
					break;

				case 0x0005:
				case 0x0007: /* mirror? baroque/samsho4 */
					if (VDP1_LOG) logerror ("Sprite List Polyline\n");
					current_sprite.ispoly = 1;
					vdp1_draw_poly_line(cliprect);
					break;

				case 0x0006:
					if (VDP1_LOG) logerror ("Sprite List Line\n");
					current_sprite.ispoly = 1;
					vdp1_draw_line(cliprect);
					break;

				case 0x0008:
/*              case 0x000b: // mirror? Bug 2 */
					if (VDP1_LOG) logerror ("Sprite List Set Command for User Clipping (%d,%d),(%d,%d)\n", current_sprite.CMDXA, current_sprite.CMDYA, current_sprite.CMDXC, current_sprite.CMDYC);
					rect_set(&vdp1.user_cliprect, current_sprite.CMDXA, current_sprite.CMDXC, current_sprite.CMDYA, current_sprite.CMDYC);
					break;

				case 0x0009:
					if (VDP1_LOG) logerror ("Sprite List Set Command for System Clipping (0,0),(%d,%d)\n", current_sprite.CMDXC, current_sprite.CMDYC);
					rect_set(&vdp1.system_cliprect, 0, current_sprite.CMDXC, 0, current_sprite.CMDYC);
					break;

				case 0x000a:
					if (VDP1_LOG) logerror ("Sprite List Local Co-Ordinate Set (%d %d)\n",(INT16)current_sprite.CMDXA,(INT16)current_sprite.CMDYA);
					vdp1.local_x = (INT16)current_sprite.CMDXA;
					vdp1.local_y = (INT16)current_sprite.CMDYA;
					break;

				default:
					/* asenna 0x0c or 0x0d (transition from title screen) */
					/* raymanj 0x0d (at startup) */
					/* choroqpk 0x0e (when selecting island in main menu) */
					/* albodysj 0x0f (always) */
					if ((current_sprite.CMDCTRL & 0x000f) < 0xc)
						popmessage ("VDP1: Sprite List Illegal %02x (%d)",current_sprite.CMDCTRL & 0xf,spritecount);
					vdp1.lopr = (position * 0x20) >> 3;
					/*vdp1.copr = (position * 0x20) >> 3; */
					/* prematurely kill the VDP1 process if an illegal opcode is executed */
					/* sexyparo calls multiple illegals and expects VDP1 irq to be fired anyway! */
					goto end;
			}
		}

	}


	end:
	vdp1.copr = (position * 0x20) >> 3;


	/* TODO: what's the exact formula? Guess it should be a mix between number of pixels written and actual command data fetched. */
	/* if spritecount = 10000 don't send a vdp1 draw end */
/*  if(spritecount < 10000) */
	timer_adjust(vdp1.draw_end_timer, TIME_IN_CYCLES(spritecount*16, 0), 0, 0);

	if (VDP1_LOG) logerror ("End of list processing!\n");
}

static void vdp1_video_update(void)
{
	int framebuffer_changed = 0;

/*  int enable; */
/*  if (machine.input().code_pressed (KEYCODE_R)) VDP1_LOG = 1; */
/*  if (machine.input().code_pressed (KEYCODE_T)) VDP1_LOG = 0; */

/*  if (machine.input().code_pressed (KEYCODE_Y)) VDP1_LOG = 0; */
/*  { */
/*      FILE *fp; */

/*      fp=fopen("vdp1_ram.dmp", "w+b"); */
/*      if (fp) */
/*      { */
/*          fwrite(stv_vdp1, 0x00100000, 1, fp); */
/*          fclose(fp); */
/*      } */
/*  } */
	if (VDP1_LOG) logerror("vdp1_video_update called\n");
	if (VDP1_LOG) logerror( "FBCR = %0x, accessed = %d\n", VDP1_FBCR, vdp1.fbcr_accessed );

	if(VDP1_CEF)
		BEF_1();
	else
		BEF_0();

	if ( vdp1.framebuffer_clear_on_next_frame )
	{
		if ( ((VDP1_FBCR & 0x3) == 3) &&
			vdp1.fbcr_accessed )
		{
			vdp1_clear_framebuffer(vdp1.framebuffer_current_display);
			vdp1.framebuffer_clear_on_next_frame = 0;
		}
	}

	switch( VDP1_FBCR & 0x3 )
	{
		case 0: /* Automatic mode */
			vdp1_change_framebuffers();
			vdp1_clear_framebuffer(vdp1.framebuffer_current_draw);
			framebuffer_changed = 1;
			break;
		case 1: /* Setting prohibited */
			break;
		case 2: /* Manual mode - erase */
			if (vdp1.fbcr_accessed)
			{
				vdp1.framebuffer_clear_on_next_frame = 1;
			}
			break;
		case 3: /* Manual mode - change */
			if (vdp1.fbcr_accessed)
			{
				vdp1_change_framebuffers();
				if (VDP1_VBE())
				{
					vdp1_clear_framebuffer(vdp1.framebuffer_current_draw);
				}
				/* TODO: Slam n Jam 96 & Cross Romance doesn't like this, investigate. */
				framebuffer_changed = 1;
			}
	/*      framebuffer_changed = 1; */
			break;
	}
	vdp1.fbcr_accessed = 0;

	if (VDP1_LOG) logerror("PTM = %0x, TVM = %x\n", VDP1_PTM, VDP1_TVM());
	/*Set CEF bit to 0*/
	/*CEF_0(); */
	switch (VDP1_PTM & 3)
	{
		case 0:/*Idle Mode*/
			/*Set CEF bit to 0*/
			/*CEF_0(); */
			break;
		case 1:/*Draw by request*/
			/*Set CEF bit to 0*/
			/*CEF_0(); */
			break;
		case 2:/*Automatic Draw*/
			if ( framebuffer_changed || VDP1_LOG )
			{
				/*set CEF to 1*/
				vdp1_process_list();
			}
			break;
		case 3: /*<invalid>*/
			logerror("Warning: Invalid PTM mode set for VDP1!\n");
			break;
	}
	/*popmessage("%04x %04x",VDP1_EWRR_X3,VDP1_EWRR_Y3); */
}

static void vdp1_state_save_postload(void)
{
	UINT8 *gfx = vdp1.gfx_decode;
	int offset;
	UINT32 data;
	int draw = vdp1.framebuffer_current_draw;

	vdp1.framebuffer_mode = -1;
	vdp1.framebuffer_double_interlace = -1;

	vdp1_set_framebuffer_config();

	/* the configuration starts from framebuffer 0: back to the saved one */
	vdp1.framebuffer_current_draw = draw;
	vdp1.framebuffer_current_display = draw ^ 1;
	vdp1_prepare_framebuffers();

	for (offset = 0; offset < 0x80000/4; offset++ )
	{
		data = vdp1_vram[offset];
		/* put in gfx region for easy decoding */
		gfx[offset*4+0] = (data & 0xff000000) >> 24;
		gfx[offset*4+1] = (data & 0x00ff0000) >> 16;
		gfx[offset*4+2] = (data & 0x0000ff00) >> 8;
		gfx[offset*4+3] = (data & 0x000000ff) >> 0;
	}
}

static int vdp1_start(void)
{
	vdp1_regs = auto_malloc(0x020);
	vdp1_vram = auto_malloc(0x100000);
	vdp1.gfx_decode = auto_malloc(0x100000);
	memset(vdp1_regs, 0, 0x020);
	memset(vdp1_vram, 0, 0x100000);
	memset(vdp1.gfx_decode, 0, 0x100000);

	vdp1_shading_data = auto_malloc(sizeof(struct vdp1_poly_scanline_data));
	memset(vdp1_shading_data, 0, sizeof(struct vdp1_poly_scanline_data));

	vdp1.framebuffer[0] = auto_malloc(1024 * 256 * 2 * sizeof(UINT16)); /* *2 is for double interlace */
	vdp1.framebuffer[1] = auto_malloc(1024 * 256 * 2 * sizeof(UINT16));
	memset(vdp1.framebuffer[0], 0, 1024 * 256 * 2 * sizeof(UINT16));
	memset(vdp1.framebuffer[1], 0, 1024 * 256 * 2 * sizeof(UINT16));

	vdp1.framebuffer_display_lines = auto_malloc(512 * sizeof(UINT16 *));
	vdp1.framebuffer_draw_lines = auto_malloc(512 * sizeof(UINT16 *));

	vdp1.framebuffer_width = vdp1.framebuffer_height = 0;
	vdp1.framebuffer_mode = -1;
	vdp1.framebuffer_double_interlace = -1;
	vdp1.fbcr_accessed = 0;
	vdp1.framebuffer_current_display = 0;
	vdp1.framebuffer_current_draw = 1;
	vdp1_clear_framebuffer(vdp1.framebuffer_current_draw);
	vdp1.framebuffer_clear_on_next_frame = 0;

	rect_set(&vdp1.system_cliprect, 0, 0, 0, 0);
	/* Kidou Senshi Z Gundam - Zenpen Zeta no Kodou loves to use the user cliprect vars in an undefined state ... */
	rect_set(&vdp1.user_cliprect, 0, 512, 0, 256);

	vdp1.draw_end_timer = timer_alloc(vdp1_draw_end);

	/* save state (with the framebuffers, unlike MAME, so a state goes on frame for frame) */
	state_save_register_UINT16("stv_vdp1", 0, "regs", vdp1_regs, 0x020/2);
	state_save_register_UINT32("stv_vdp1", 0, "vram", vdp1_vram, 0x100000/4);
	state_save_register_UINT16("stv_vdp1", 0, "fb0", vdp1.framebuffer[0], 1024 * 256 * 2);
	state_save_register_UINT16("stv_vdp1", 0, "fb1", vdp1.framebuffer[1], 1024 * 256 * 2);
	state_save_register_int("stv_vdp1", 0, "fbcr_accessed", &vdp1.fbcr_accessed);
	state_save_register_int("stv_vdp1", 0, "fb_display", &vdp1.framebuffer_current_display);
	state_save_register_int("stv_vdp1", 0, "fb_draw", &vdp1.framebuffer_current_draw);
	state_save_register_int("stv_vdp1", 0, "fb_clear", &vdp1.framebuffer_clear_on_next_frame);
	state_save_register_int("stv_vdp1", 0, "local_x", &vdp1.local_x);
	state_save_register_int("stv_vdp1", 0, "local_y", &vdp1.local_y);
	state_save_register_UINT16("stv_vdp1", 0, "lopr", &vdp1.lopr, 1);
	state_save_register_UINT16("stv_vdp1", 0, "copr", &vdp1.copr, 1);
	state_save_register_UINT16("stv_vdp1", 0, "ewdr", &vdp1.ewdr, 1);
	state_save_register_func_postload(vdp1_state_save_postload);
	return 0;
}


/**********************************************************************************************************************/

/* Sega Saturn VDP2 */

/*

-------------------------- WARNING WARNING WARNING --------------------------
This is a legacy core, all game based notes are for a future device rewrite.
Please don't remove them if for no reason you truly want to mess with this.
-------------------------- WARNING WARNING WARNING --------------------------

the dirty marking stuff and tile decoding will probably be removed in the end anyway as we'll need custom
rendering code since mame's drawgfx / tilesytem don't offer everything st-v needs

this system seems far too complex to use Mame's tilemap system

4 'scroll' planes (scroll screens)

the scroll planes have slightly different capabilities

NBG0
NBG1
NBG2
NBG3

2 'rotate' planes

RBG0
RBG1

-- other crap
EXBG (external)

-----------------------------------------------------------------------------------------------------------

Video emulation TODO:
-all games:
 \-priorities (check myfairld,thunt)
 \-complete windows effects
 \-mosaic effect
 \-ODD bit/H/V Counter not yet emulated properly
 \-Reduction enable bits (zooming limiters)
 \-Check if there are any remaining video registers that are yet to be macroized & added to the rumble.
-batmanfr:
 \-If you reset the game after the character selection screen,when you get again to it there's garbage
   floating behind Batman.
-elandore:
 \-(BTANB) priorities at the VS. screen apparently is wrong,but it's like this on the Saturn version too.
-hanagumi:
 \-ending screens have corrupt graphics. (*untested*)
-kiwames:
 \-(fixed) incorrect color emulation for the alpha blended flames on the title screen,it's caused by a schizoid
   linescroll emulation quirk.
 \-the VDP1 sprites refresh is too slow,causing the "Draw by request" mode to
   flicker. Moved back to default ATM.
-pblbeach:
 \-Sprites are offset, because it doesn't clear vdp1 local coordinates set by bios,
   I guess that they are cleared when some vdp1 register is written (kludged for now)
-prikura:
 \-Attract mode presentation has corrupted graphics in various places,probably caused by incomplete
   framebuffer data delete.
-seabass:
 \-(fixed) Player sprite is corrupt/missing during movements,caused by incomplete framebuffer switching.
-shienryu:
 \-level 2 background colors on statues, caused by special color calculation usage (per dot);
(Saturn games)
- scud the disposable assassin:
 \- when zooming on melee attack background gets pink, color calculation issue?
- virtual hydlide:
 \- transparent pens usage on most vdp1 items should be black instead.
 \- likewise "press start button" is the other way around, i.e. black pen where it should be transparent instead.

Notes of Interest & Unclear features:

-the test mode / bios is drawn with layer NBG3;
-hanagumi puts a 'RED' dragon logo in tileram (base 0x64000, 4bpp, 8x8 tiles) but
its not displayed because its priority value is 0.Left-over?

-scrolling is screen display wise,meaning that a scrolling value is masked with the
screen resolution size values;

-H-Blank bit is INDIPENDENT of the V-Blank bit...trying to fix enable/disable it during V-Blank period
 causes wrong gameplay speed in Golden Axe:The Duel.

-Bitmaps uses transparency pens,examples are:
\-elandore's energy bars;
\-mausuke's foreground(the one used on the playfield)
\-shanhigw's tile-based sprites;
The transparency pen table is like this:

|------------------|---------------------|
| Character count  | Transparency code   |
|------------------|---------------------|
| 16 colors        |=0x0 (4 bits)        |
| 256 colors       |=0x00 (8 bits)       |
| 2048 colors      |=0x000 (11 bits)     |
| 32,768 colors    |MSB=0 (bit 15)       |
| 16,770,000 colors|MSB=0 (bit 31)       |
|------------------|---------------------|
In other words,the first three types uses the offset and not the color allocated.

-double density interlace setting (LSMD == 3) apparently does a lot of fancy stuff in the graphics sizes.

-Debug key list(only if you enable the debug mode on top of this file):
    \-T: NBG3 layer toggle
    \-Y: NBG2 layer toggle
    \-U: NBG1 layer toggle
    \-I: NBG0 layer toggle
    \-O: SPRITE toggle
    \-K: RBG0 layer toggle
    \-W Decodes the graphics for F4 menu.
    \-M Stores VDP1 ram contents from a file.
    \-N Stores VDP1 ram contents into a file.
*/

#define TEST_FUNCTIONS 0
#define POPMESSAGE_DEBUG 0


enum
{
	STV_TRANSPARENCY_PEN = 0x0,
	STV_TRANSPARENCY_NONE = 0x1,
	STV_TRANSPARENCY_ADD_BLEND = 0x2,
	STV_TRANSPARENCY_ALPHA = 0x4
};

#define DEBUG_DRAW_ROZ (0)

/*

-------------------------------------------------|-----------------------------|------------------------------
|  Function        |  Normal Scroll Screen                                     |  Rotation Scroll Screen     |
|                  |-----------------------------|-----------------------------|------------------------------
|                  | NBG0         | NBG1         | NBG2         | NBG3         | RBG0         | RBG1         |
-------------------------------------------------|-----------------------------|------------------------------
| Character Colour | 16 colours   | 16 colours   | 16 colours   | 16 colours   | 16 colours   | 16 colours   |
| Count            | 256 " "      | 256 " "      | 256 " "      | 256 " "      | 256 " "      | 256 " "      |
|                  | 2048 " "     | 2048 " "     |              |              | 2048 " "     | 2048 " "     |
|                  | 32768 " "    | 32768 " "    |              |              | 32768 " "    | 32768 " "    |
|                  | 16770000 " " |              |              |              | 16770000 " " | 16770000 " " |
-------------------------------------------------|-----------------------------|------------------------------
| Character Size   | 1x1 Cells , 2x2 Cells                                                                   |
-------------------------------------------------|-----------------------------|------------------------------
| Pattern Name     | 1 word , 2 words                                                                        |
| Data Size        |                                                                                         |
-------------------------------------------------|-----------------------------|------------------------------
| Plane Size       | 1 H x 1 V 1 Pages ; 2 H x 1 V 1 Pages ; 2 H x 2 V Pages                                 |
-------------------------------------------------|-----------------------------|------------------------------
| Plane Count      | 4                                                         | 16                          |
-------------------------------------------------|-----------------------------|------------------------------
| Bitmap Possible  | Yes                         | No                          | Yes          | No           |
-------------------------------------------------|-----------------------------|------------------------------
| Bitmap Size      | 512 x 256                   | N/A                         | 512x256      | N/A          |
|                  | 512 x 512                   |                             | 512x512      |              |
|                  | 1024 x 256                  |                             |              |              |
|                  | 1024 x 512                  |                             |              |              |
-------------------------------------------------|-----------------------------|------------------------------
| Scale            | 0.25 x - 256 x              | None                        | Any ?                       |
-------------------------------------------------|-----------------------------|------------------------------
| Rotation         | No                                                        | Yes                         |
-------------------------------------------------|-----------------------------|-----------------------------|
| Linescroll       | Yes                         | No                                                        |
-------------------------------------------------|-----------------------------|------------------------------
| Column Scroll    | Yes                         | No                                                        |
-------------------------------------------------|-----------------------------|------------------------------
| Mosaic           | Yes                                                       | Horizontal Only             |
-------------------------------------------------|-----------------------------|------------------------------

*/



/* 18000C - RESERVED
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

/* 18000E - r/w - RAMCTL - RAM Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |  CRKTE   |    --    | CRMD1    | CRMD0    |    --    |    --    | VRBMD    | VRAMD    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | RDBSB11  | RDBSB10  | RDBSB01  | RDBSB00  | RDBSA11  | RDBSA10  | RDBSA01  | RDBSA00  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_RAMCTL (vdp2_regs[0x00e/2])

	#define VDP2_CRKTE ((VDP2_RAMCTL & 0x8000) >> 15)
	#define VDP2_CRMD  ((VDP2_RAMCTL & 0x3000) >> 12)
	#define VDP2_RDBSB1 ((VDP2_RAMCTL & 0x00c0) >> 6)
	#define VDP2_RDBSB0 ((VDP2_RAMCTL & 0x0030) >> 4)
	#define VDP2_RDBSA1 ((VDP2_RAMCTL & 0x000c) >> 2)
	#define VDP2_RDBSA0 ((VDP2_RAMCTL & 0x0003) >> 0)


/* 180010 - r/w - -CYCA0L - VRAM CYCLE PATTERN (BANK A0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP0A03  | VCP0A02  | VCP0A01  | VCP0A00  | VCP1A03  | VCP1A02  | VCP1A01  | VCP1A00  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP2A03  | VCP2A02  | VCP2A01  | VCP2A00  | VCP3A03  | VCP3A02  | VCP3A01  | VCP3A00  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA0L (vdp2_regs[0x010/2])

/* 180012 - r/w - -CYCA0U - VRAM CYCLE PATTERN (BANK A0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP4A03  | VCP4A02  | VCP4A01  | VCP4A00  | VCP5A03  | VCP5A02  | VCP5A01  | VCP5A00  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP6A03  | VCP6A02  | VCP6A01  | VCP6A00  | VCP7A03  | VCP7A02  | VCP7A01  | VCP7A00  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA0U (vdp2_regs[0x012/2])

/* 180014 - r/w - -CYCA1L - VRAM CYCLE PATTERN (BANK A1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP0A13  | VCP0A12  | VCP0A11  | VCP0A10  | VCP1A13  | VCP1A12  | VCP1A11  | VCP1A10  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP2A13  | VCP2A12  | VCP2A11  | VCP2A10  | VCP3A13  | VCP3A12  | VCP3A11  | VCP3A10  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA1L (vdp2_regs[0x014/2])

/* 180016 - r/w - -CYCA1U - VRAM CYCLE PATTERN (BANK A1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP4A13  | VCP4A12  | VCP4A11  | VCP4A10  | VCP5A13  | VCP5A12  | VCP5A11  | VCP5A10  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP6A13  | VCP6A12  | VCP6A11  | VCP6A10  | VCP7A13  | VCP7A12  | VCP7A11  | VCP7A10  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA1U (vdp2_regs[0x016/2])

/* 180018 - r/w - -CYCB0L - VRAM CYCLE PATTERN (BANK B0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP0B03  | VCP0B02  | VCP0B01  | VCP0B00  | VCP1B03  | VCP1B02  | VCP1B01  | VCP1B00  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP2B03  | VCP2B02  | VCP2B01  | VCP2B00  | VCP3B03  | VCP3B02  | VCP3B01  | VCP3B00  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA2L (vdp2_regs[0x018/2])

/* 18001A - r/w - -CYCB0U - VRAM CYCLE PATTERN (BANK B0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP4B03  | VCP4B02  | VCP4B01  | VCP4B00  | VCP5B03  | VCP5B02  | VCP5B01  | VCP5B00  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP6B03  | VCP6B02  | VCP6B01  | VCP6B00  | VCP7B03  | VCP7B02  | VCP7B01  | VCP7B00  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA2U (vdp2_regs[0x01a/2])

/* 18001C - r/w - -CYCB1L - VRAM CYCLE PATTERN (BANK B1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP0B13  | VCP0B12  | VCP0B11  | VCP0B10  | VCP1B13  | VCP1B12  | VCP1B11  | VCP1B10  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP2B13  | VCP2B12  | VCP2B11  | VCP2B10  | VCP3B13  | VCP3B12  | VCP3B11  | VCP3B10  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA3L (vdp2_regs[0x01c/2])

/* 18001E - r/w - -CYCB1U - VRAM CYCLE PATTERN (BANK B1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | VCP4B13  | VCP4B12  | VCP4B11  | VCP4B10  | VCP5B13  | VCP5B12  | VCP5B11  | VCP5B10  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | VCP6B13  | VCP6B12  | VCP6B11  | VCP6B10  | VCP7B13  | VCP7B12  | VCP7B11  | VCP7B10  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CYCA3U (vdp2_regs[0x01e/2])

/* 180020 - r/w - BGON - SCREEN DISPLAY ENABLE

 this register allows each tilemap to be enabled or disabled and also which layers are solid

 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    | R0TPON   | N3TPON   | N2TPON   | N1TPON   | N0TPON   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    | R1ON     | R0ON     | N3ON     | N2ON     | N1ON     | N0ON     |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_BGON (vdp2_regs[0x020/2])

	/* NxOn - Layer Enable Register */
	#define VDP2_xxON ((VDP2_BGON & 0x001f) >> 0) /* to see if anything is enabled */

	#define VDP2_N0ON ((VDP2_BGON & 0x0001) >> 0) /* N0On = NBG0 Enable */
	#define VDP2_N1ON ((VDP2_BGON & 0x0002) >> 1) /* N1On = NBG1 Enable */
	#define VDP2_N2ON ((VDP2_BGON & 0x0004) >> 2) /* N2On = NBG2 Enable */
	#define VDP2_N3ON ((VDP2_BGON & 0x0008) >> 3) /* N3On = NBG3 Enable */
	#define VDP2_R0ON ((VDP2_BGON & 0x0010) >> 4) /* R0On = RBG0 Enable */
	#define VDP2_R1ON ((VDP2_BGON & 0x0020) >> 5) /* R1On = RBG1 Enable */

	/* NxTPON - Transparency Pen Enable Registers */
	#define VDP2_N0TPON ((VDP2_BGON & 0x0100) >> 8) /*  N0TPON = NBG0 Draw Transparent Pen (as solid) /or/ RBG1 Draw Transparent Pen */
	#define VDP2_N1TPON ((VDP2_BGON & 0x0200) >> 9) /*  N1TPON = NBG1 Draw Transparent Pen (as solid) /or/ EXBG Draw Transparent Pen */
	#define VDP2_N2TPON ((VDP2_BGON & 0x0400) >> 10)/*  N2TPON = NBG2 Draw Transparent Pen (as solid) */
	#define VDP2_N3TPON ((VDP2_BGON & 0x0800) >> 11)/*  N3TPON = NBG3 Draw Transparent Pen (as solid) */
	#define VDP2_R0TPON ((VDP2_BGON & 0x1000) >> 12)/*  R0TPON = RBG0 Draw Transparent Pen (as solid) */

/*
180022 - MZCTL - Mosaic Control
bit->  /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MZCTL (vdp2_regs[0x022/2])

	#define VDP2_MZSZV ((VDP2_MZCTL & 0xf000) >> 12)
	#define VDP2_MZSZH ((VDP2_MZCTL & 0x0f00) >> 8)
	#define VDP2_R0MZE ((VDP2_MZCTL & 0x0010) >> 4)
	#define VDP2_N3MZE ((VDP2_MZCTL & 0x0008) >> 3)
	#define VDP2_N2MZE ((VDP2_MZCTL & 0x0004) >> 2)
	#define VDP2_N1MZE ((VDP2_MZCTL & 0x0002) >> 1)
	#define VDP2_N0MZE ((VDP2_MZCTL & 0x0001) >> 0)

/*180024 - Special Function Code Select

*/

	#define VDP2_SFSEL (vdp2_regs[0x024/2])

/*180026 - Special Function Code

*/

	#define VDP2_SFCODE (vdp2_regs[0x026/2])


/*
180028 - CHCTLA - Character Control (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    | N1CHCN1  | N1CHCN0  | N1BMSZ1  | N1BMSZ0  | N1BMEN   | N1CHSZ   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    | N0CHCN2  | N0CHCN1  | N0CHCN0  | N0BMSZ1  | N0BMSZ0  | N0BMEN   | N0CHSZ   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CHCTLA (vdp2_regs[0x028/2])

/* -------------------------- NBG0 Character Control Registers -------------------------- */

/*  N0CHCNx  NBG0 (or RGB1) Colour Depth
    000 - 16 Colours
    001 - 256 Colours
    010 - 2048 Colours
    011 - 32768 Colours (RGB5)
    100 - 16770000 Colours (RGB8)
    101 - invalid
    110 - invalid
    111 - invalid   */
	#define VDP2_N0CHCN ((VDP2_CHCTLA & 0x0070) >> 4)

/*  N0BMSZx - NBG0 Bitmap Size *guessed*
    00 - 512 x 256
    01 - 512 x 512
    10 - 1024 x 256
    11 - 1024 x 512   */
	#define VDP2_N0BMSZ ((VDP2_CHCTLA & 0x000c) >> 2)

/*  N0BMEN - NBG0 Bitmap Enable
    0 - use cell mode
    1 - use bitmap mode   */
	#define VDP2_N0BMEN ((VDP2_CHCTLA & 0x0002) >> 1)

/*  N0CHSZ - NBG0 Character (Tile) Size
    0 - 1 cell  x 1 cell  (8x8)
    1 - 2 cells x 2 cells (16x16)  */
	#define VDP2_N0CHSZ ((VDP2_CHCTLA & 0x0001) >> 0)

/* -------------------------- NBG1 Character Control Registers -------------------------- */

/*  N1CHCNx - NBG1 (or EXB1) Colour Depth
    00 - 16 Colours
    01 - 256 Colours
    10 - 2048 Colours
    11 - 32768 Colours (RGB5)  */
	#define VDP2_N1CHCN ((VDP2_CHCTLA & 0x3000) >> 12)

/*  N1BMSZx - NBG1 Bitmap Size *guessed*
    00 - 512 x 256
    01 - 512 x 512
    10 - 1024 x 256
    11 - 1024 x 512   */
	#define VDP2_N1BMSZ ((VDP2_CHCTLA & 0x0c00) >> 10)

/*  N1BMEN - NBG1 Bitmap Enable
    0 - use cell mode
    1 - use bitmap mode   */
	#define VDP2_N1BMEN ((VDP2_CHCTLA & 0x0200) >> 9)

/*  N1CHSZ - NBG1 Character (Tile) Size
    0 - 1 cell  x 1 cell  (8x8)
    1 - 2 cells x 2 cells (16x16)  */
	#define VDP2_N1CHSZ ((VDP2_CHCTLA & 0x0100) >> 8)

/*
18002A - CHCTLB - Character Control (NBG2, NBG1, RBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    | R0CHCN2  | R0CHCN1  | R0CHCN0  |    --    | R0BMSZ   | R0BMEN   | R0CHSZ   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    | N3CHCN   | N3CHSZ   |    --    |    --    | N2CHCN   | N2CHSZ   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CHCTLB (vdp2_regs[0x02a/2])

/* -------------------------- RBG0 Character Control Registers -------------------------- */


/*  R0CHCNx  RBG0  Colour Depth
    000 - 16 Colours
    001 - 256 Colours
    010 - 2048 Colours
    011 - 32768 Colours (RGB5)
    100 - 16770000 Colours (RGB8)
    101 - invalid
    110 - invalid
    111 - invalid   */
	#define VDP2_R0CHCN ((VDP2_CHCTLB & 0x7000) >> 12)

/*  R0BMSZx - RBG0 Bitmap Size *guessed*
    00 - 512 x 256
    01 - 512 x 512  */
	#define VDP2_R0BMSZ ((VDP2_CHCTLB & 0x0400) >> 10)

/*  R0BMEN - RBG0 Bitmap Enable
    0 - use cell mode
    1 - use bitmap mode   */
	#define VDP2_R0BMEN ((VDP2_CHCTLB & 0x0200) >> 9)

/*  R0CHSZ - RBG0 Character (Tile) Size
    0 - 1 cell  x 1 cell  (8x8)
    1 - 2 cells x 2 cells (16x16)  */
	#define VDP2_R0CHSZ ((VDP2_CHCTLB & 0x0100) >> 8)

	#define VDP2_N3CHCN ((VDP2_CHCTLB & 0x0020) >> 5)
	#define VDP2_N3CHSZ ((VDP2_CHCTLB & 0x0010) >> 4)
	#define VDP2_N2CHCN ((VDP2_CHCTLB & 0x0002) >> 1)
	#define VDP2_N2CHSZ ((VDP2_CHCTLB & 0x0001) >> 0)


/*
18002C - BMPNA - Bitmap Palette Number (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_BMPNA (vdp2_regs[0x02c/2])

	#define VDP2_N1BMP ((VDP2_BMPNA & 0x0700) >> 8)
	#define VDP2_N0BMP ((VDP2_BMPNA & 0x0007) >> 0)

/* 18002E - Bitmap Palette Number (RBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_BMPNB (vdp2_regs[0x02e/2])

	#define VDP2_R0BMP ((VDP2_BMPNB & 0x0007) >> 0)

/* 180030 - PNCN0 - Pattern Name Control (NBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | N0PNB    | N0CNSM   |    --    |    --    |    --    |    --    | N0SPR    | N0SCC    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | N0SPLT6  | N0SPLT5  | N0SPLT4  | N0SPCN4  | N0SPCN3  | N0SPCN2  | N0SPCN1  | N0SPCN0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PNCN0 (vdp2_regs[0x030/2])

/*  Pattern Data Size
    0 = 2 bytes
    1 = 1 byte */
	#define VDP2_N0PNB  ((VDP2_PNCN0 & 0x8000) >> 15)

/*  Character Number Supplement (in 1 byte mode)
    0 = Character Number = 10bits + 2bits for flip
    1 = Character Number = 12 bits, no flip  */
	#define VDP2_N0CNSM ((VDP2_PNCN0 & 0x4000) >> 14)

/*  NBG0 Special Priority Register (in 1 byte mode) */
	#define VDP2_N0SPR ((VDP2_PNCN0 & 0x0200) >> 9)

/*  NBG0 Special Colour Control Register (in 1 byte mode) */
	#define VDP2_N0SCC ((VDP2_PNCN0 & 0x0100) >> 8)

/*  Supplementary Palette Bits (in 1 byte mode) */
	#define VDP2_N0SPLT ((VDP2_PNCN0 & 0x00e0) >> 5)

/*  Supplementary Character Bits (in 1 byte mode) */
	#define VDP2_N0SPCN ((VDP2_PNCN0 & 0x001f) >> 0)

/* 180032 - Pattern Name Control (NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PNCN1 (vdp2_regs[0x032/2])

/*  Pattern Data Size
    0 = 2 bytes
    1 = 1 byte */
	#define VDP2_N1PNB  ((VDP2_PNCN1 & 0x8000) >> 15)

/*  Character Number Supplement (in 1 byte mode)
    0 = Character Number = 10bits + 2bits for flip
    1 = Character Number = 12 bits, no flip  */
	#define VDP2_N1CNSM ((VDP2_PNCN1 & 0x4000) >> 14)

/*  NBG0 Special Priority Register (in 1 byte mode) */
	#define VDP2_N1SPR ((VDP2_PNCN1 & 0x0200) >> 9)

/*  NBG0 Special Colour Control Register (in 1 byte mode) */
	#define VDP2_N1SCC ((VDP2_PNCN1 & 0x0100) >> 8)

/*  Supplementary Palette Bits (in 1 byte mode) */
	#define VDP2_N1SPLT ((VDP2_PNCN1 & 0x00e0) >> 5)

/*  Supplementary Character Bits (in 1 byte mode) */
	#define VDP2_N1SPCN ((VDP2_PNCN1 & 0x001f) >> 0)


/* 180034 - Pattern Name Control (NBG2)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PNCN2 (vdp2_regs[0x034/2])

/*  Pattern Data Size
    0 = 2 bytes
    1 = 1 byte */
	#define VDP2_N2PNB  ((VDP2_PNCN2 & 0x8000) >> 15)

/*  Character Number Supplement (in 1 byte mode)
    0 = Character Number = 10bits + 2bits for flip
    1 = Character Number = 12 bits, no flip  */
	#define VDP2_N2CNSM ((VDP2_PNCN2 & 0x4000) >> 14)

/*  NBG0 Special Priority Register (in 1 byte mode) */
	#define VDP2_N2SPR ((VDP2_PNCN2 & 0x0200) >> 9)

/*  NBG0 Special Colour Control Register (in 1 byte mode) */
	#define VDP2_N2SCC ((VDP2_PNCN2 & 0x0100) >> 8)

/*  Supplementary Palette Bits (in 1 byte mode) */
	#define VDP2_N2SPLT ((VDP2_PNCN2 & 0x00e0) >> 5)

/*  Supplementary Character Bits (in 1 byte mode) */
	#define VDP2_N2SPCN ((VDP2_PNCN2 & 0x001f) >> 0)


/* 180036 - Pattern Name Control (NBG3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       | N3PNB    | N3CNSM   |    --    |    --    |    --    |    --    | N3SPR    | N3SCC    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | N3SPLT6  | N3SPLT5  | N3SPLT4  | N3SPCN4  | N3SPCN3  | N3SPCN2  | N3SPCN1  | N3SPCN0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PNCN3 (vdp2_regs[0x036/2])

/*  Pattern Data Size
    0 = 2 bytes
    1 = 1 byte */
	#define VDP2_N3PNB  ((VDP2_PNCN3 & 0x8000) >> 15)

/*  Character Number Supplement (in 1 byte mode)
    0 = Character Number = 10bits + 2bits for flip
    1 = Character Number = 12 bits, no flip  */
	#define VDP2_N3CNSM ((VDP2_PNCN3 & 0x4000) >> 14)

/*  NBG0 Special Priority Register (in 1 byte mode) */
	#define VDP2_N3SPR ((VDP2_PNCN3 & 0x0200) >> 9)

/*  NBG0 Special Colour Control Register (in 1 byte mode) */
	#define VDP2_N3SCC ((VDP2_PNCN3 & 0x0100) >> 8)

/*  Supplementary Palette Bits (in 1 byte mode) */
	#define VDP2_N3SPLT ((VDP2_PNCN3 & 0x00e0) >> 5)

/*  Supplementary Character Bits (in 1 byte mode) */
	#define VDP2_N3SPCN ((VDP2_PNCN3 & 0x001f) >> 0)


/* 180038 - Pattern Name Control (RBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PNCR (vdp2_regs[0x038/2])

/*  Pattern Data Size
    0 = 2 bytes
    1 = 1 byte */
	#define VDP2_R0PNB  ((VDP2_PNCR & 0x8000) >> 15)

/*  Character Number Supplement (in 1 byte mode)
    0 = Character Number = 10bits + 2bits for flip
    1 = Character Number = 12 bits, no flip  */
	#define VDP2_R0CNSM ((VDP2_PNCR & 0x4000) >> 14)

/*  NBG0 Special Priority Register (in 1 byte mode) */
	#define VDP2_R0SPR ((VDP2_PNCR & 0x0200) >> 9)

/*  NBG0 Special Colour Control Register (in 1 byte mode) */
	#define VDP2_R0SCC ((VDP2_PNCR & 0x0100) >> 8)

/*  Supplementary Palette Bits (in 1 byte mode) */
	#define VDP2_R0SPLT ((VDP2_PNCR & 0x00e0) >> 5)

/*  Supplementary Character Bits (in 1 byte mode) */
	#define VDP2_R0SPCN ((VDP2_PNCR & 0x001f) >> 0)

/* 18003A - PLSZ - Plane Size
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       | N3PLSZ1  | N3PLSZ0  |    --    |    --    | N1PLSZ1  | N1PLSZ0  | N0PLSZ1  | N0PLSZ0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PLSZ (vdp2_regs[0x03a/2])

	/* NBG0 Plane Size
	00 1H Page x 1V Page
	01 2H Pages x 1V Page
	10 invalid
	11 2H Pages x 2V Pages  */
	#define VDP2_RBOVR  ((VDP2_PLSZ & 0xc000) >> 14)
	#define VDP2_RBPLSZ ((VDP2_PLSZ & 0x3000) >> 12)
	#define VDP2_RAOVR  ((VDP2_PLSZ & 0x0c00) >> 10)
	#define VDP2_RAPLSZ ((VDP2_PLSZ & 0x0300) >> 8)
	#define VDP2_N3PLSZ ((VDP2_PLSZ & 0x00c0) >> 6)
	#define VDP2_N2PLSZ ((VDP2_PLSZ & 0x0030) >> 4)
	#define VDP2_N1PLSZ ((VDP2_PLSZ & 0x000c) >> 2)
	#define VDP2_N0PLSZ ((VDP2_PLSZ & 0x0003) >> 0)

/* 18003C - MPOFN - Map Offset (NBG0, NBG1, NBG2, NBG3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    | N3MP8    | N3MP7    | N3MP6    |    --    | N2MP8    | N2MP7    | N2MP6    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    | N1MP8    | N1MP7    | N1MP6    |    --    | N0MP8    | N0MP7    | N0MP6    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPOFN_ (vdp2_regs[0x03c/2])

	/* Higher 3 bits of the map offset for each layer */
	#define VDP2_N3MP_ ((VDP2_MPOFN_ & 0x3000) >> 12)
	#define VDP2_N2MP_ ((VDP2_MPOFN_ & 0x0300) >> 8)
	#define VDP2_N1MP_ ((VDP2_MPOFN_ & 0x0030) >> 4)
	#define VDP2_N0MP_ ((VDP2_MPOFN_ & 0x0003) >> 0)




/* 18003E - Map Offset (Rotation Parameter A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPOFR_ (vdp2_regs[0x03e/2])

	#define VDP2_RBMP_ ((VDP2_MPOFR_ & 0x0030) >> 4)
	#define VDP2_RAMP_ ((VDP2_MPOFR_ & 0x0003) >> 0)

/* 180040 - MPABN0 - Map (NBG0, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    | N0MPB5   | N0MPB4   | N0MPB3   | N0MPB2   | N0MPB1   | N0MPB0   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    | N0MPA5   | N0MPA4   | N0MPA3   | N0MPA2   | N0MPA1   | N0MPA0   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABN0 (vdp2_regs[0x040/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap NBG0 */
	#define VDP2_N0MPB ((VDP2_MPABN0 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap NBG0 */
	#define VDP2_N0MPA ((VDP2_MPABN0 & 0x003f) >> 0)


/* 180042 - MPCDN0 - (NBG0, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    | N0MPD5   | N0MPD4   | N0MPD3   | N0MPD2   | N0MPD1   | N0MPD0   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    | N0MPC5   | N0MPC4   | N0MPC3   | N0MPC2   | N0MPC1   | N0MPC0   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPCDN0 (vdp2_regs[0x042/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane D of Tilemap NBG0 */
	#define VDP2_N0MPD ((VDP2_MPCDN0 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane C of Tilemap NBG0 */
	#define VDP2_N0MPC ((VDP2_MPCDN0 & 0x003f) >> 0)


/* 180044 - Map (NBG1, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABN1 (vdp2_regs[0x044/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap NBG1 */
	#define VDP2_N1MPB ((VDP2_MPABN1 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap NBG1 */
	#define VDP2_N1MPA ((VDP2_MPABN1 & 0x003f) >> 0)

/* 180046 - Map (NBG1, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPCDN1 (vdp2_regs[0x046/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane D of Tilemap NBG0 */
	#define VDP2_N1MPD ((VDP2_MPCDN1 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane C of Tilemap NBG0 */
	#define VDP2_N1MPC ((VDP2_MPCDN1 & 0x003f) >> 0)


/* 180048 - Map (NBG2, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABN2 (vdp2_regs[0x048/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap NBG2 */
	#define VDP2_N2MPB ((VDP2_MPABN2 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap NBG2 */
	#define VDP2_N2MPA ((VDP2_MPABN2 & 0x003f) >> 0)

/* 18004a - Map (NBG2, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPCDN2 (vdp2_regs[0x04a/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane D of Tilemap NBG2 */
	#define VDP2_N2MPD ((VDP2_MPCDN2 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane C of Tilemap NBG2 */
	#define VDP2_N2MPC ((VDP2_MPCDN2 & 0x003f) >> 0)

/* 18004c - Map (NBG3, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABN3 (vdp2_regs[0x04c/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap NBG1 */
	#define VDP2_N3MPB ((VDP2_MPABN3 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap NBG1 */
	#define VDP2_N3MPA ((VDP2_MPABN3 & 0x003f) >> 0)


/* 18004e - Map (NBG3, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPCDN3 (vdp2_regs[0x04e/2])

	/* N0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap NBG0 */
	#define VDP2_N3MPD ((VDP2_MPCDN3 & 0x3f00) >> 8)

	/* N0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap NBG0 */
	#define VDP2_N3MPC ((VDP2_MPCDN3 & 0x003f) >> 0)

/* 180050 - Map (Rotation Parameter A, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABRA (vdp2_regs[0x050/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap RBG0 */
	#define VDP2_RAMPB ((VDP2_MPABRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap RBG0 */
	#define VDP2_RAMPA ((VDP2_MPABRA & 0x003f) >> 0)



/* 180052 - Map (Rotation Parameter A, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPCDRA (vdp2_regs[0x052/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane D of Tilemap RBG0 */
	#define VDP2_RAMPD ((VDP2_MPCDRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane C of Tilemap RBG0 */
	#define VDP2_RAMPC ((VDP2_MPCDRA & 0x003f) >> 0)

/* 180054 - Map (Rotation Parameter A, Plane E,F)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPEFRA (vdp2_regs[0x054/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane F of Tilemap RBG0 */
	#define VDP2_RAMPF ((VDP2_MPEFRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane E of Tilemap RBG0 */
	#define VDP2_RAMPE ((VDP2_MPEFRA & 0x003f) >> 0)

/* 180056 - Map (Rotation Parameter A, Plane G,H)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPGHRA (vdp2_regs[0x056/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane H of Tilemap RBG0 */
	#define VDP2_RAMPH ((VDP2_MPGHRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane G of Tilemap RBG0 */
	#define VDP2_RAMPG ((VDP2_MPGHRA & 0x003f) >> 0)

/* 180058 - Map (Rotation Parameter A, Plane I,J)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPIJRA (vdp2_regs[0x058/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane J of Tilemap RBG0 */
	#define VDP2_RAMPJ ((VDP2_MPIJRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane I of Tilemap RBG0 */
	#define VDP2_RAMPI ((VDP2_MPIJRA & 0x003f) >> 0)

/* 18005a - Map (Rotation Parameter A, Plane K,L)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPKLRA (vdp2_regs[0x05a/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane L of Tilemap RBG0 */
	#define VDP2_RAMPL ((VDP2_MPKLRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane K of Tilemap RBG0 */
	#define VDP2_RAMPK ((VDP2_MPKLRA & 0x003f) >> 0)

/* 18005c - Map (Rotation Parameter A, Plane M,N)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPMNRA (vdp2_regs[0x05c/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane N of Tilemap RBG0 */
	#define VDP2_RAMPN ((VDP2_MPMNRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane M of Tilemap RBG0 */
	#define VDP2_RAMPM ((VDP2_MPMNRA & 0x003f) >> 0)

/* 18005e - Map (Rotation Parameter A, Plane O,P)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_MPOPRA (vdp2_regs[0x05e/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane P of Tilemap RBG0 */
	#define VDP2_RAMPP ((VDP2_MPOPRA & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane O of Tilemap RBG0 */
	#define VDP2_RAMPO ((VDP2_MPOPRA & 0x003f) >> 0)

/* 180060 - Map (Rotation Parameter B, Plane A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPABRB (vdp2_regs[0x060/2])

	/* R0MPB5 = lower 6 bits of Map Address of Plane B of Tilemap RBG0 */
	#define VDP2_RBMPB ((VDP2_MPABRB & 0x3f00) >> 8)

	/* R0MPA5 = lower 6 bits of Map Address of Plane A of Tilemap RBG0 */
	#define VDP2_RBMPA ((VDP2_MPABRB & 0x003f) >> 0)


/* 180062 - Map (Rotation Parameter B, Plane C,D)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPCDRB (vdp2_regs[0x062/2])

	/* R0MPD5 = lower 6 bits of Map Address of Plane D of Tilemap RBG0 */
	#define VDP2_RBMPD ((VDP2_MPCDRB & 0x3f00) >> 8)

	/* R0MPc5 = lower 6 bits of Map Address of Plane C of Tilemap RBG0 */
	#define VDP2_RBMPC ((VDP2_MPCDRB & 0x003f) >> 0)

/* 180064 - Map (Rotation Parameter B, Plane E,F)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPEFRB (vdp2_regs[0x064/2])

	/* R0MPF5 = lower 6 bits of Map Address of Plane F of Tilemap RBG0 */
	#define VDP2_RBMPF ((VDP2_MPEFRB & 0x3f00) >> 8)

	/* R0MPE5 = lower 6 bits of Map Address of Plane E of Tilemap RBG0 */
	#define VDP2_RBMPE ((VDP2_MPEFRB & 0x003f) >> 0)

/* 180066 - Map (Rotation Parameter B, Plane G,H)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPGHRB (vdp2_regs[0x066/2])

	/* R0MPH5 = lower 6 bits of Map Address of Plane H of Tilemap RBG0 */
	#define VDP2_RBMPH ((VDP2_MPGHRB & 0x3f00) >> 8)

	/* R0MPG5 = lower 6 bits of Map Address of Plane G of Tilemap RBG0 */
	#define VDP2_RBMPG ((VDP2_MPGHRB & 0x003f) >> 0)

/* 180068 - Map (Rotation Parameter B, Plane I,J)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPIJRB (vdp2_regs[0x068/2])

	/* R0MPJ5 = lower 6 bits of Map Address of Plane J of Tilemap RBG0 */
	#define VDP2_RBMPJ ((VDP2_MPIJRB & 0x3f00) >> 8)

	/* R0MPI5 = lower 6 bits of Map Address of Plane E of Tilemap RBG0 */
	#define VDP2_RBMPI ((VDP2_MPIJRB & 0x003f) >> 0)

/* 18006a - Map (Rotation Parameter B, Plane K,L)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPKLRB (vdp2_regs[0x06a/2])

	/* R0MPL5 = lower 6 bits of Map Address of Plane L of Tilemap RBG0 */
	#define VDP2_RBMPL ((VDP2_MPKLRB & 0x3f00) >> 8)

	/* R0MPK5 = lower 6 bits of Map Address of Plane K of Tilemap RBG0 */
	#define VDP2_RBMPK ((VDP2_MPKLRB & 0x003f) >> 0)

/* 18006c - Map (Rotation Parameter B, Plane M,N)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPMNRB (vdp2_regs[0x06c/2])

	/* R0MPN5 = lower 6 bits of Map Address of Plane N of Tilemap RBG0 */
	#define VDP2_RBMPN ((VDP2_MPMNRB & 0x3f00) >> 8)

	/* R0MPM5 = lower 6 bits of Map Address of Plane M of Tilemap RBG0 */
	#define VDP2_RBMPM ((VDP2_MPMNRB & 0x003f) >> 0)

/* 18006e - Map (Rotation Parameter B, Plane O,P)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_MPOPRB (vdp2_regs[0x06e/2])

	/* R0MPP5 = lower 6 bits of Map Address of Plane P of Tilemap RBG0 */
	#define VDP2_RBMPP ((VDP2_MPOPRB & 0x3f00) >> 8)

	/* R0MPO5 = lower 6 bits of Map Address of Plane O of Tilemap RBG0 */
	#define VDP2_RBMPO ((VDP2_MPOPRB & 0x003f) >> 0)

/* 180070 - SCXIN0 - Screen Scroll (NBG0, Horizontal Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXIN0 (vdp2_regs[0x070/2])


/* 180072 - Screen Scroll (NBG0, Horizontal Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXDN0 (vdp2_regs[0x072/2])

/* 180074 - SCYIN0 - Screen Scroll (NBG0, Vertical Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_SCYIN0 (vdp2_regs[0x074/2])


/* 180076 - Screen Scroll (NBG0, Vertical Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCYDN0 (vdp2_regs[0x076/2])

/* 180078 - Coordinate Inc (NBG0, Horizontal Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMXIN0 (vdp2_regs[0x078/2])

	#define VDP2_N0ZMXI ((VDP2_ZMXIN0 & 0x0007) >> 0)

/* 18007a - Coordinate Inc (NBG0, Horizontal Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMXDN0 (vdp2_regs[0x07a/2])

	#define VDP2_N0ZMXD ((VDP2_ZMXDN0 >> 8)& 0xff)
	#define VDP2_ZMXN0  (((VDP2_N0ZMXI<<16) | (VDP2_N0ZMXD<<8))  & 0x0007ff00)


/* 18007c - Coordinate Inc (NBG0, Vertical Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMYIN0 (vdp2_regs[0x07c/2])

	#define VDP2_N0ZMYI ((VDP2_ZMYIN0 & 0x0007) >> 0)

/* 18007e - Coordinate Inc (NBG0, Vertical Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMYDN0 (vdp2_regs[0x07e/2])

	#define VDP2_N0ZMYD ((VDP2_ZMYDN0 >> 8)& 0xff)
	#define VDP2_ZMYN0  (((VDP2_N0ZMYI<<16) | (VDP2_N0ZMYD<<8))  & 0x0007ff00)

/* 180080 - SCXIN1 - Screen Scroll (NBG1, Horizontal Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXIN1 (vdp2_regs[0x080/2])

/* 180082 - Screen Scroll (NBG1, Horizontal Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXDN1 (vdp2_regs[0x082/2])

/* 180084 - SCYIN1 - Screen Scroll (NBG1, Vertical Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCYIN1 (vdp2_regs[0x084/2])

/* 180086 - Screen Scroll (NBG1, Vertical Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCYDN1 (vdp2_regs[0x086/2])

/* 180088 - Coordinate Inc (NBG1, Horizontal Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMXIN1 (vdp2_regs[0x088/2])

	#define VDP2_N1ZMXI ((VDP2_ZMXIN1 & 0x0007) >> 0)

/* 18008a - Coordinate Inc (NBG1, Horizontal Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMXDN1 (vdp2_regs[0x08a/2])

	#define VDP2_N1ZMXD ((VDP2_ZMXDN1 >> 8)& 0xff)
	#define VDP2_ZMXN1  (((VDP2_N1ZMXI<<16) | (VDP2_N1ZMXD<<8)) & 0x0007ff00)

/* 18008c - Coordinate Inc (NBG1, Vertical Integer Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMYIN1 (vdp2_regs[0x08c/2])

	#define VDP2_N1ZMYI ((VDP2_ZMYIN1 & 0x0007) >> 0)

/* 18008e - Coordinate Inc (NBG1, Vertical Fractional Part)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMYDN1 (vdp2_regs[0x08e/2])

	#define VDP2_N1ZMYD ((VDP2_ZMYDN1 >> 8)& 0xff)
	#define VDP2_ZMYN1  (((VDP2_N1ZMYI<<16) | (VDP2_N1ZMYD<<8)) & 0x007ff00)

/* 180090 - SCXN2 - Screen Scroll (NBG2, Horizontal)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXN2 (vdp2_regs[0x090/2])

/* 180092 - SCYN2 - Screen Scroll (NBG2, Vertical)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCYN2 (vdp2_regs[0x092/2])

/* 180094 - SCXN3 - Screen Scroll (NBG3, Horizontal)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCXN3 (vdp2_regs[0x094/2])

/* 180096 - SCYN3 - Screen Scroll (NBG3, Vertical)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCYN3 (vdp2_regs[0x096/2])

/* 180098 - Reduction Enable
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    | N1ZMQT   | N1ZMHF   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    | N0ZMQT   | N0ZMHF   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_ZMCTL (vdp2_regs[0x098/2])

	#define VDP2_N1ZMQT  ((VDP2_ZMCTL & 0x0200) >> 9)
	#define VDP2_N1ZMHF  ((VDP2_ZMCTL & 0x0100) >> 8)
	#define VDP2_N0ZMQT  ((VDP2_ZMCTL & 0x0002) >> 1)
	#define VDP2_N0ZMHF  ((VDP2_ZMCTL & 0x0001) >> 0)

/* 18009a - Line and Vertical Cell Scroll Control (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SCRCTL (vdp2_regs[0x09a/2])

	#define VDP2_N1LSS  ((VDP2_SCRCTL & 0x3000) >> 12)
	#define VDP2_N1LZMX ((VDP2_SCRCTL & 0x0800) >> 11)
	#define VDP2_N1LSCY ((VDP2_SCRCTL & 0x0400) >> 10)
	#define VDP2_N1LSCX ((VDP2_SCRCTL & 0x0200) >> 9)
	#define VDP2_N1VCSC ((VDP2_SCRCTL & 0x0100) >> 8)
	#define VDP2_N0LSS  ((VDP2_SCRCTL & 0x0030) >> 4)
	#define VDP2_N0LZMX ((VDP2_SCRCTL & 0x0008) >> 3)
	#define VDP2_N0LSCY ((VDP2_SCRCTL & 0x0004) >> 2)
	#define VDP2_N0LSCX ((VDP2_SCRCTL & 0x0002) >> 1)
	#define VDP2_N0VCSC ((VDP2_SCRCTL & 0x0001) >> 0)

/* 18009c - Vertical Cell Table Address (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_VCSTAU (vdp2_regs[0x09c/2] & 7)


/* 18009e - Vertical Cell Table Address (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_VCSTAL (vdp2_regs[0x09e/2])

/* 1800a0 - LSTA0U - Line Scroll Table Address (NBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	/*bit 2 unused when VRAM = 4 Mbits*/
	#define VDP2_LSTA0U (vdp2_regs[0x0a0/2] & 7)

/* 1800a2 - LSTA0L - Line Scroll Table Address (NBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LSTA0L (vdp2_regs[0x0a2/2])

/* 1800a4 - LSTA1U - Line Scroll Table Address (NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	/*bit 2 unused when VRAM = 4 Mbits*/
	#define VDP2_LSTA1U (vdp2_regs[0x0a4/2] & 7)

/* 1800a6 - LSTA1L - Line Scroll Table Address (NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LSTA1L (vdp2_regs[0x0a6/2])

/* 1800a8 - LCTAU - Line Colour Screen Table Address
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LCTAU  (vdp2_regs[0x0a8/2])
	#define VDP2_LCCLMD ((VDP2_LCTAU & 0x8000) >> 15)

/* 1800aa - LCTAL - Line Colour Screen Table Address
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_LCTAL  (vdp2_regs[0x0aa/2])

	#define VDP2_LCTA   (((VDP2_LCTAU & 0x0007) << 16) | (VDP2_LCTAL & 0xffff))

/* 1800ac - Back Screen Table Address
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |  BKCLMD  |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  BKTA18  |  BKTA17  |  BKTA16  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_BKTAU  (vdp2_regs[0x0ac/2])

	#define VDP2_BKCLMD ((VDP2_BKTAU & 0x8000) >> 15)


/* 1800ae - Back Screen Table Address
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |  BKTA15  |  BKTA14  |  BKTA13  |  BKTA12  |  BKTA11  |  BKTA10  |  BKTA9   |  BKTA8   |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |  BKTA7   |  BKTA7   |  BKTA6   |  BKTA5   |  BKTA4   |  BKTA3   |  BKTA2   |  BKTA0   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_BKTAL  (vdp2_regs[0x0ae/2])

	#define VDP2_BKTA   (((VDP2_BKTAU & 0x0007) << 16) | (VDP2_BKTAL & 0xffff))

/* 1800b0 - RPMD - Rotation Parameter Mode
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_RPMD   ((vdp2_regs[0x0b0/2]) & 0x0003)

/* 1800b2 - RPRCTL - Rotation Parameter Read Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    | RBKASTRE | RBYSTRE  | RBXSTRE  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    | RAKASTRE | RAYSTRE  | RBXSTRE  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_RPRCTL     (vdp2_regs[0x0b2/2])
	#define VDP2_RBKASTRE   ((VDP2_RPRCTL & 0x0400) >> 10)
	#define VDP2_RBYSTRE    ((VDP2_RPRCTL & 0x0200) >> 9)
	#define VDP2_RBXSTRE    ((VDP2_RPRCTL & 0x0100) >> 8)
	#define VDP2_RAKASTRE   ((VDP2_RPRCTL & 0x0004) >> 2)
	#define VDP2_RAYSTRE    ((VDP2_RPRCTL & 0x0002) >> 1)
	#define VDP2_RAXSTRE    ((VDP2_RPRCTL & 0x0001) >> 0)

/* 1800b4 - KTCTL - Coefficient Table Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |  RBKLCE  |  RBKMD1  |  RBKMD0  |  RBKDBS  |   RBKTE  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |  RAKLCE  |  RAKMD1  |  RAKMD0  |  RAKDBS  |   RAKTE  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_KTCTL  (vdp2_regs[0x0b4/2])
	#define VDP2_RBUNK  ((VDP2_KTCTL & 0x6000) >> 13)
	#define VDP2_RBKLCE ((VDP2_KTCTL & 0x1000) >> 12)
	#define VDP2_RBKMD  ((VDP2_KTCTL & 0x0c00) >> 10)
	#define VDP2_RBKDBS ((VDP2_KTCTL & 0x0200) >> 9)
	#define VDP2_RBKTE  ((VDP2_KTCTL & 0x0100) >> 8)
	#define VDP2_RAUNK  ((VDP2_KTCTL & 0x0060) >> 5)
	#define VDP2_RAKLCE ((VDP2_KTCTL & 0x0010) >> 4)
	#define VDP2_RAKMD  ((VDP2_KTCTL & 0x000c) >> 2)
	#define VDP2_RAKDBS ((VDP2_KTCTL & 0x0002) >> 1)
	#define VDP2_RAKTE  ((VDP2_KTCTL & 0x0001) >> 0)

/* 1800b6 - KTAOF - Coefficient Table Address Offset (Rotation Parameter A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    | RBKTAOS2 | RBKTAOS1 | RBKTAOS0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    | RAKTAOS2 | RAKTAOS1 | RAKTAOS0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_KTAOF  (vdp2_regs[0x0b6/2])
	#define VDP2_RBKTAOS ((VDP2_KTAOF & 0x0700) >> 8)
	#define VDP2_RAKTAOS ((VDP2_KTAOF & 0x0007) >> 0)

/* 1800b8 - OVPNRA - Screen Over Pattern Name (Rotation Parameter A)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_OVPNRA (vdp2_regs[0x0b8/2])

/* 1800ba - Screen Over Pattern Name (Rotation Parameter B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_OVPNRB (vdp2_regs[0x0ba/2])

/* 1800bc - RPTAU - Rotation Parameter Table Address (Rotation Parameter A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  RPTA18  |  RPTA17  |  RPTA16  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_RPTAU  (vdp2_regs[0x0bc/2] & 7)

/* 1800be - RPTAL - Rotation Parameter Table Address (Rotation Parameter A,B)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |  RPTA15  |  RPTA14  |  RPTA13  |  RPTA12  |  RPTA11  |  RPTA10  |   RPTA9  |   RPTA8  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |   RPTA7  |   RPTA6  |   RPTA5  |   RPTA4  |   RPTA3  |   RPTA2  |   RPTA1  |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_RPTAL  (vdp2_regs[0x0be/2] & 0x0000ffff)

/* 1800c0 - Window Position (W0, Horizontal Start Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPSX0 (vdp2_regs[0x0c0/2])

	#define VDP2_W0SX ((VDP2_WPSX0 & 0x03ff) >> 0)

/* 1800c2 - Window Position (W0, Vertical Start Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPSY0 (vdp2_regs[0x0c2/2])

	#define VDP2_W0SY ((VDP2_WPSY0 & 0x07ff) >> 0)

/* 1800c4 - Window Position (W0, Horizontal End Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPEX0 (vdp2_regs[0x0c4/2])

	#define VDP2_W0EX ((VDP2_WPEX0 & 0x03ff) >> 0)

/* 1800c6 - Window Position (W0, Vertical End Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPEY0 (vdp2_regs[0x0c6/2])

	#define VDP2_W0EY ((VDP2_WPEY0 & 0x07ff) >> 0)

/* 1800c8 - Window Position (W1, Horizontal Start Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPSX1 (vdp2_regs[0x0c8/2])

	#define VDP2_W1SX ((VDP2_WPSX1 & 0x03ff) >> 0)

/* 1800ca - Window Position (W1, Vertical Start Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPSY1 (vdp2_regs[0x0ca/2])

	#define VDP2_W1SY ((VDP2_WPSY1 & 0x07ff) >> 0)

/* 1800cc - Window Position (W1, Horizontal End Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPEX1 (vdp2_regs[0x0cc/2])

	#define VDP2_W1EX ((VDP2_WPEX1 & 0x03ff) >> 0)

/* 1800ce - Window Position (W1, Vertical End Point)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WPEY1 (vdp2_regs[0x0ce/2])

	#define VDP2_W1EY ((VDP2_WPEY1 & 0x07ff) >> 0)

/* 1800d0 - Window Control (NBG0, NBG1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WCTLA (vdp2_regs[0x0d0/2])
	#define VDP2_N1LOG ((VDP2_WCTLA & 0x8000) >> 15)
	#define VDP2_N1SWE ((VDP2_WCTLA & 0x2000) >> 13)
	#define VDP2_N1SWA ((VDP2_WCTLA & 0x1000) >> 12)
	#define VDP2_N1W1E ((VDP2_WCTLA & 0x0800) >> 11)
	#define VDP2_N1W1A ((VDP2_WCTLA & 0x0400) >> 10)
	#define VDP2_N1W0E ((VDP2_WCTLA & 0x0200) >> 9)
	#define VDP2_N1W0A ((VDP2_WCTLA & 0x0100) >> 8)
	#define VDP2_N0LOG ((VDP2_WCTLA & 0x0080) >> 7)
	#define VDP2_N0SWE ((VDP2_WCTLA & 0x0020) >> 5)
	#define VDP2_N0SWA ((VDP2_WCTLA & 0x0010) >> 4)
	#define VDP2_N0W1E ((VDP2_WCTLA & 0x0008) >> 3)
	#define VDP2_N0W1A ((VDP2_WCTLA & 0x0004) >> 2)
	#define VDP2_N0W0E ((VDP2_WCTLA & 0x0002) >> 1)
	#define VDP2_N0W0A ((VDP2_WCTLA & 0x0001) >> 0)

/* 1800d2 - Window Control (NBG2, NBG3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WCTLB (vdp2_regs[0x0d2/2])
	#define VDP2_N3LOG ((VDP2_WCTLB & 0x8000) >> 15)
	#define VDP2_N3SWE ((VDP2_WCTLB & 0x2000) >> 13)
	#define VDP2_N3SWA ((VDP2_WCTLB & 0x1000) >> 12)
	#define VDP2_N3W1E ((VDP2_WCTLB & 0x0800) >> 11)
	#define VDP2_N3W1A ((VDP2_WCTLB & 0x0400) >> 10)
	#define VDP2_N3W0E ((VDP2_WCTLB & 0x0200) >> 9)
	#define VDP2_N3W0A ((VDP2_WCTLB & 0x0100) >> 8)
	#define VDP2_N2LOG ((VDP2_WCTLB & 0x0080) >> 7)
	#define VDP2_N2SWE ((VDP2_WCTLB & 0x0020) >> 5)
	#define VDP2_N2SWA ((VDP2_WCTLB & 0x0010) >> 4)
	#define VDP2_N2W1E ((VDP2_WCTLB & 0x0008) >> 3)
	#define VDP2_N2W1A ((VDP2_WCTLB & 0x0004) >> 2)
	#define VDP2_N2W0E ((VDP2_WCTLB & 0x0002) >> 1)
	#define VDP2_N2W0A ((VDP2_WCTLB & 0x0001) >> 0)

/* 1800d4 - Window Control (RBG0, Sprite)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WCTLC (vdp2_regs[0x0d4/2])
	#define VDP2_SPLOG ((VDP2_WCTLC & 0x8000) >> 15)
	#define VDP2_SPSWE ((VDP2_WCTLC & 0x2000) >> 13)
	#define VDP2_SPSWA ((VDP2_WCTLC & 0x1000) >> 12)
	#define VDP2_SPW1E ((VDP2_WCTLC & 0x0800) >> 11)
	#define VDP2_SPW1A ((VDP2_WCTLC & 0x0400) >> 10)
	#define VDP2_SPW0E ((VDP2_WCTLC & 0x0200) >> 9)
	#define VDP2_SPW0A ((VDP2_WCTLC & 0x0100) >> 8)
	#define VDP2_R0LOG ((VDP2_WCTLC & 0x0080) >> 7)
	#define VDP2_R0SWE ((VDP2_WCTLC & 0x0020) >> 5)
	#define VDP2_R0SWA ((VDP2_WCTLC & 0x0010) >> 4)
	#define VDP2_R0W1E ((VDP2_WCTLC & 0x0008) >> 3)
	#define VDP2_R0W1A ((VDP2_WCTLC & 0x0004) >> 2)
	#define VDP2_R0W0E ((VDP2_WCTLC & 0x0002) >> 1)
	#define VDP2_R0W0A ((VDP2_WCTLC & 0x0001) >> 0)

/* 1800d6 - Window Control (Parameter Window, Colour Calc. Window)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_WCTLD (vdp2_regs[0x0d6/2])
	#define VDP2_CCLOG ((VDP2_WCTLD & 0x8000) >> 15)
	#define VDP2_CCSWE ((VDP2_WCTLD & 0x2000) >> 13)
	#define VDP2_CCSWA ((VDP2_WCTLD & 0x1000) >> 12)
	#define VDP2_CCW1E ((VDP2_WCTLD & 0x0800) >> 11)
	#define VDP2_CCW1A ((VDP2_WCTLD & 0x0400) >> 10)
	#define VDP2_CCW0E ((VDP2_WCTLD & 0x0200) >> 9)
	#define VDP2_CCW0A ((VDP2_WCTLD & 0x0100) >> 8)
	#define VDP2_RPLOG ((VDP2_WCTLD & 0x0080) >> 7)
	#define VDP2_RPW1E ((VDP2_WCTLD & 0x0008) >> 3)
	#define VDP2_RPW1A ((VDP2_WCTLD & 0x0004) >> 2)
	#define VDP2_RPW0E ((VDP2_WCTLD & 0x0002) >> 1)
	#define VDP2_RPW0A ((VDP2_WCTLD & 0x0001) >> 0)

/* 1800d8 - Line Window Table Address (W0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LWTA0U (vdp2_regs[0x0d8/2])

	#define VDP2_W0LWE  ((VDP2_LWTA0U & 0x8000) >> 15)

/* 1800da - Line Window Table Address (W0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LWTA0L (vdp2_regs[0x0da/2])

	/* bit 19 isn't used when VRAM = 4 Mbit */
	#define VDP2_W0LWTA (((VDP2_LWTA0U & 0x0007) << 16) | (VDP2_LWTA0L & 0xfffe))


/* 1800dc - Line Window Table Address (W1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LWTA1U (vdp2_regs[0x0dc/2])

	#define VDP2_W1LWE  ((VDP2_LWTA1U & 0x8000) >> 15)


/* 1800de - Line Window Table Address (W1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LWTA1L (vdp2_regs[0x0de/2])

	/* bit 19 isn't used when VRAM = 4 Mbit */
	#define VDP2_W1LWTA (((VDP2_LWTA1U & 0x0007) << 16) | (VDP2_LWTA1L & 0xfffe))


/* 1800e0 - Sprite Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    | SPCCCS1  | SPCCCS0  |    --    |  SPCCN2  |  SPCCN1  |  SPCCN0  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |  SPCLMD  | SPWINEN  |  SPTYPE3 |  SPTYPE2 |  SPTYPE1 |  SPTYPE0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SPCTL  (vdp2_regs[0x0e0/2])
	#define VDP2_SPCCCS     ((VDP2_SPCTL & 0x3000) >> 12)
	#define VDP2_SPCCN      ((VDP2_SPCTL & 0x700) >> 8)
	#define VDP2_SPCLMD     ((VDP2_SPCTL & 0x20) >> 5)
	#define VDP2_SPWINEN    ((VDP2_SPCTL & 0x10) >> 4)
	#define VDP2_SPTYPE     (VDP2_SPCTL & 0xf)

/* 1800e2 - Shadow Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SDCTL  (vdp2_regs[0x0e2/2])

/* 1800e4 - CRAOFA - Colour Ram Address Offset (NBG0 - NBG3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    | N0CAOS2  | N3CAOS1  | N3CAOS0  |    --    | N2CAOS2  | N2CAOS1  | N2CAOS0  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    | N1CAOS2  | N1CAOS1  | N1CAOS0  |    --    | N0CAOS2  | N0CAOS1  | N0CAOS0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CRAOFA (vdp2_regs[0x0e4/2])

	/* NxCAOS =  */
	#define VDP2_N0CAOS ((VDP2_CRAOFA & 0x0007) >> 0)
	#define VDP2_N1CAOS ((VDP2_CRAOFA & 0x0070) >> 4)
	#define VDP2_N2CAOS ((VDP2_CRAOFA & 0x0700) >> 8)
	#define VDP2_N3CAOS ((VDP2_CRAOFA & 0x7000) >> 12)


/* 1800e6 - Colour Ram Address Offset (RBG0, SPRITE)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_CRAOFB (vdp2_regs[0x0e6/2])
	#define VDP2_R0CAOS ((VDP2_CRAOFB & 0x0007) >> 0)
	#define VDP2_SPCAOS ((VDP2_CRAOFB & 0x0070) >> 4)

/* 1800e8 - LNCLEN - Line Colour Screen Enable
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |  SPLCEN  |  R0LCEN  |  N3LCEN  |  N2LCEN  |  N1LCEN  | N0LCEN   |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_LNCLEN (vdp2_regs[0x0e8/2])
	#define VDP2_SPLCEN ((VDP2_LNCLEN & 0x0020) >> 5)
	#define VDP2_R0LCEN ((VDP2_LNCLEN & 0x0010) >> 4)
	#define VDP2_N3LCEN ((VDP2_LNCLEN & 0x0008) >> 3)
	#define VDP2_N2LCEN ((VDP2_LNCLEN & 0x0004) >> 2)
	#define VDP2_N1LCEN ((VDP2_LNCLEN & 0x0002) >> 1)
	#define VDP2_N0LCEN ((VDP2_LNCLEN & 0x0001) >> 0)

/* 1800ea - Special Priority Mode
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SFPRMD (vdp2_regs[0x0ea/2])


/* 1800ec - Colour Calculation Control
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |  BOKEN   |  BOKN2   |  BOKN1   |   BOKN0  |    --    |  EXCCEN  |  CCRTMD  |  CCMD    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |  SPCCEN  |  LCCCEN  |  R0CCEN  |  N3CCEN  |  N2CCEN  |  N1CCEN  |  N0CCEN  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCCR       (vdp2_regs[0x0ec/2])
	#define VDP2_CCMD       ((VDP2_CCCR & 0x100) >> 8)
	#define VDP2_SPCCEN     ((VDP2_CCCR & 0x40) >> 6)
	#define VDP2_LCCCEN     ((VDP2_CCCR & 0x20) >> 5)
	#define VDP2_R0CCEN     ((VDP2_CCCR & 0x10) >> 4)
	#define VDP2_N3CCEN     ((VDP2_CCCR & 0x8) >> 3)
	#define VDP2_N2CCEN     ((VDP2_CCCR & 0x4) >> 2)
	#define VDP2_N1CCEN     ((VDP2_CCCR & 0x2) >> 1)
	#define VDP2_N0CCEN     ((VDP2_CCCR & 0x1) >> 0)


/* 1800ee - Special Colour Calculation Mode
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_SFCCMD     (vdp2_regs[0x0ee/2])

/* 1800f0 - Priority Number (Sprite 0,1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |  S1PRIN2 |  S1PRIN1 |  S1PRIN0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  S0PRIN2 |  S0PRIN1 |  S0PRIN0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRISA      (vdp2_regs[0x0f0/2])
	#define VDP2_S1PRIN     ((VDP2_PRISA & 0x0700) >> 8)
	#define VDP2_S0PRIN     ((VDP2_PRISA & 0x0007) >> 0)

/* 1800f2 - Priority Number (Sprite 2,3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |  S3PRIN2 |  S3PRIN1 |  S3PRIN0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  S2PRIN2 |  S2PRIN1 |  S2PRIN0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRISB      (vdp2_regs[0x0f2/2])
	#define VDP2_S3PRIN     ((VDP2_PRISB & 0x0700) >> 8)
	#define VDP2_S2PRIN     ((VDP2_PRISB & 0x0007) >> 0)

/* 1800f4 - Priority Number (Sprite 4,5)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |  S5PRIN2 |  S5PRIN1 |  S5PRIN0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  S4PRIN2 |  S4PRIN1 |  S4PRIN0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRISC      (vdp2_regs[0x0f4/2])
	#define VDP2_S5PRIN     ((VDP2_PRISC & 0x0700) >> 8)
	#define VDP2_S4PRIN     ((VDP2_PRISC & 0x0007) >> 0)

/* 1800f6 - Priority Number (Sprite 6,7)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |  S7PRIN2 |  S7PRIN1 |  S7PRIN0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |  S6PRIN2 |  S6PRIN1 |  S6PRIN0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRISD      (vdp2_regs[0x0f6/2])
	#define VDP2_S7PRIN     ((VDP2_PRISD & 0x0700) >> 8)
	#define VDP2_S6PRIN     ((VDP2_PRISD & 0x0007) >> 0)


/* 1800f8 - PRINA - Priority Number (NBG 0,1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRINA (vdp2_regs[0x0f8/2])

	#define VDP2_N1PRIN ((VDP2_PRINA & 0x0700) >> 8)
	#define VDP2_N0PRIN ((VDP2_PRINA & 0x0007) >> 0)

/* 1800fa - PRINB - Priority Number (NBG 2,3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_PRINB (vdp2_regs[0x0fa/2])

	#define VDP2_N3PRIN ((VDP2_PRINB & 0x0700) >> 8)
	#define VDP2_N2PRIN ((VDP2_PRINB & 0x0007) >> 0)

/* 1800fc - Priority Number (RBG0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_PRIR (vdp2_regs[0x0fc/2])

	#define VDP2_R0PRIN ((VDP2_PRIR & 0x0007) >> 0)

/* 1800fe - Reserved
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

/* 180100 - Colour Calculation Ratio (Sprite 0,1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |  S1CCRT4 |  S1CCRT3 |  S1CCRT2 |  S1CCRT1 |  S1CCRT0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |  S0CCRT4 |  S0CCRT3 |  S0CCRT2 |  S0CCRT1 |  S0CCRT0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRSA      (vdp2_regs[0x100/2])
	#define VDP2_S1CCRT     ((VDP2_CCRSA & 0x1f00) >> 8)
	#define VDP2_S0CCRT     ((VDP2_CCRSA & 0x001f) >> 0)

/* 180102 - Colour Calculation Ratio (Sprite 2,3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |  S3CCRT4 |  S3CCRT3 |  S3CCRT2 |  S3CCRT1 |  S3CCRT0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |  S2CCRT4 |  S2CCRT3 |  S2CCRT2 |  S2CCRT1 |  S2CCRT0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRSB      (vdp2_regs[0x102/2])
	#define VDP2_S3CCRT     ((VDP2_CCRSB & 0x1f00) >> 8)
	#define VDP2_S2CCRT     ((VDP2_CCRSB & 0x001f) >> 0)

/* 180104 - Colour Calculation Ratio (Sprite 4,5)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |  S5CCRT4 |  S5CCRT3 |  S5CCRT2 |  S5CCRT1 |  S5CCRT0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |  S4CCRT4 |  S4CCRT3 |  S4CCRT2 |  S4CCRT1 |  S4CCRT0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRSC      (vdp2_regs[0x104/2])
	#define VDP2_S5CCRT     ((VDP2_CCRSC & 0x1f00) >> 8)
	#define VDP2_S4CCRT     ((VDP2_CCRSC & 0x001f) >> 0)

/* 180106 - Colour Calculation Ratio (Sprite 6,7)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |  S7CCRT4 |  S7CCRT3 |  S7CCRT2 |  S7CCRT1 |  S7CCRT0 |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |  S6CCRT4 |  S6CCRT3 |  S6CCRT2 |  S6CCRT1 |  S6CCRT0 |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRSD      (vdp2_regs[0x106/2])
	#define VDP2_S7CCRT     ((VDP2_CCRSD & 0x1f00) >> 8)
	#define VDP2_S6CCRT     ((VDP2_CCRSD & 0x001f) >> 0)

/* 180108 - Colour Calculation Ratio (NBG 0,1)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    | N1CCRT4  | N1CCRT3  | N1CCRT2  | N1CCRT1  | N1CCRT0  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    | N0CCRT4  | N0CCRT3  | N0CCRT2  | N0CCRT1  | N0CCRT0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRNA  (vdp2_regs[0x108/2])
	#define VDP2_N1CCRT ((VDP2_CCRNA & 0x1f00) >> 8)
	#define VDP2_N0CCRT (VDP2_CCRNA & 0x1f)

/* 18010a - Colour Calculation Ratio (NBG 2,3)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    | N3CCRT4  | N3CCRT3  | N3CCRT2  | N3CCRT1  | N3CCRT0  |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    | N2CCRT4  | N2CCRT3  | N2CCRT2  | N2CCRT1  | N2CCRT0  |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRNB  (vdp2_regs[0x10a/2])
	#define VDP2_N3CCRT ((VDP2_CCRNB & 0x1f00) >> 8)
	#define VDP2_N2CCRT (VDP2_CCRNB & 0x1f)

/* 18010c - Colour Calculation Ratio (RBG 0)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRR   (vdp2_regs[0x10c/2])
	#define VDP2_R0CCRT (VDP2_CCRR & 0x1f)

/* 18010e - Colour Calculation Ratio (Line Colour Screen, Back Colour Screen)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CCRLB   (vdp2_regs[0x10e/2])


/* 180110 - Colour Offset Enable
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CLOFEN (vdp2_regs[0x110/2])
	#define VDP2_N0COEN ((VDP2_CLOFEN & 0x01) >> 0)
	#define VDP2_N1COEN ((VDP2_CLOFEN & 0x02) >> 1)
	#define VDP2_N2COEN ((VDP2_CLOFEN & 0x04) >> 2)
	#define VDP2_N3COEN ((VDP2_CLOFEN & 0x08) >> 3)
	#define VDP2_R0COEN ((VDP2_CLOFEN & 0x10) >> 4)
	#define VDP2_BKCOEN ((VDP2_CLOFEN & 0x20) >> 5)
	#define VDP2_SPCOEN ((VDP2_CLOFEN & 0x40) >> 6)

/* 180112 - Colour Offset Select
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_CLOFSL (vdp2_regs[0x112/2])
	#define VDP2_N0COSL ((VDP2_CLOFSL & 0x01) >> 0)
	#define VDP2_N1COSL ((VDP2_CLOFSL & 0x02) >> 1)
	#define VDP2_N2COSL ((VDP2_CLOFSL & 0x04) >> 2)
	#define VDP2_N3COSL ((VDP2_CLOFSL & 0x08) >> 3)
	#define VDP2_R0COSL ((VDP2_CLOFSL & 0x10) >> 4)
	#define VDP2_BKCOSL ((VDP2_CLOFSL & 0x20) >> 5)
	#define VDP2_SPCOSL ((VDP2_CLOFSL & 0x40) >> 6)

/* 180114 - Colour Offset A (Red)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_COAR (vdp2_regs[0x114/2])

/* 180116 - Colour Offset A (Green)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_COAG (vdp2_regs[0x116/2])

/* 180118 - Colour Offset A (Blue)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/

	#define VDP2_COAB (vdp2_regs[0x118/2])

/* 18011a - Colour Offset B (Red)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_COBR (vdp2_regs[0x11a/2])

/* 18011c - Colour Offset B (Green)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_COBG (vdp2_regs[0x11c/2])

/* 18011e - Colour Offset B (Blue)
 bit-> /----15----|----14----|----13----|----12----|----11----|----10----|----09----|----08----\
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       |----07----|----06----|----05----|----04----|----03----|----02----|----01----|----00----|
       |    --    |    --    |    --    |    --    |    --    |    --    |    --    |    --    |
       \----------|----------|----------|----------|----------|----------|----------|---------*/
	#define VDP2_COBB (vdp2_regs[0x11e/2])


#define VDP2_RBG_ROTATION_PARAMETER_A   1
#define VDP2_RBG_ROTATION_PARAMETER_B   2


#define mul_fixed32( a, b ) mul_32x32_shift( a, b, 16 )

static void vdp2_fill_rotation_parameter_table( UINT8 rot_parameter )
{
	UINT32 address;

	address = (((VDP2_RPTAU << 16) | VDP2_RPTAL) << 1);
	if ( rot_parameter == 1 )
	{
		address &= ~0x00000080;
	}
	else if ( rot_parameter == 2 )
	{
		address |= 0x00000080;
	}

	current_rotation_table.xst  = (vdp2_vram[address/4] & 0x1fffffc0) | ((vdp2_vram[address/4] & 0x10000000) ? 0xe0000000 : 0x00000000 );
	current_rotation_table.yst  = (vdp2_vram[address/4 + 1] & 0x1fffffc0) | ((vdp2_vram[address/4 + 1] & 0x10000000) ? 0xe0000000 : 0x00000000 );
	current_rotation_table.zst  = (vdp2_vram[address/4 + 2] & 0x1fffffc0) | ((vdp2_vram[address/4 + 2] & 0x10000000) ? 0xe0000000 : 0x00000000 );
	current_rotation_table.dxst = (vdp2_vram[address/4 + 3] & 0x0007ffc0) | ((vdp2_vram[address/4 + 3] & 0x00040000) ? 0xfff80000 : 0x00000000 );
	current_rotation_table.dyst = (vdp2_vram[address/4 + 4] & 0x0007ffc0) | ((vdp2_vram[address/4 + 4] & 0x00040000) ? 0xfff80000 : 0x00000000 );
	current_rotation_table.dx   = (vdp2_vram[address/4 + 5] & 0x0007ffc0) | ((vdp2_vram[address/4 + 5] & 0x00040000) ? 0xfff80000 : 0x00000000 );
	current_rotation_table.dy   = (vdp2_vram[address/4 + 6] & 0x0007ffc0) | ((vdp2_vram[address/4 + 6] & 0x00040000) ? 0xfff80000 : 0x00000000 );
	current_rotation_table.A    = (vdp2_vram[address/4 + 7] & 0x000fffc0) | ((vdp2_vram[address/4 + 7] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.B    = (vdp2_vram[address/4 + 8] & 0x000fffc0) | ((vdp2_vram[address/4 + 8] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.C    = (vdp2_vram[address/4 + 9] & 0x000fffc0) | ((vdp2_vram[address/4 + 9] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.D    = (vdp2_vram[address/4 + 10] & 0x000fffc0) | ((vdp2_vram[address/4 + 10] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.E    = (vdp2_vram[address/4 + 11] & 0x000fffc0) | ((vdp2_vram[address/4 + 11] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.F    = (vdp2_vram[address/4 + 12] & 0x000fffc0) | ((vdp2_vram[address/4 + 12] & 0x00080000) ? 0xfff00000 : 0x00000000 );
	current_rotation_table.px   = (vdp2_vram[address/4 + 13] & 0x3fff0000) | ((vdp2_vram[address/4 + 13] & 0x30000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.py   = (vdp2_vram[address/4 + 13] & 0x00003fff) << 16;
	if ( current_rotation_table.py & 0x20000000 ) current_rotation_table.py |= 0xc0000000;
	current_rotation_table.pz   = (vdp2_vram[address/4 + 14] & 0x3fff0000) | ((vdp2_vram[address/4 + 14] & 0x20000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.cx   = (vdp2_vram[address/4 + 15] & 0x3fff0000) | ((vdp2_vram[address/4 + 15] & 0x20000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.cy   = (vdp2_vram[address/4 + 15] & 0x00003fff) << 16;
	if ( current_rotation_table.cy & 0x20000000 ) current_rotation_table.cy |= 0xc0000000;
	current_rotation_table.cz   = (vdp2_vram[address/4 + 16] & 0x3fff0000) | ((vdp2_vram[address/4 + 16] & 0x20000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.mx   = (vdp2_vram[address/4 + 17] & 0x3fffffc0) | ((vdp2_vram[address/4 + 17] & 0x20000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.my   = (vdp2_vram[address/4 + 18] & 0x3fffffc0) | ((vdp2_vram[address/4 + 18] & 0x20000000) ? 0xc0000000 : 0x00000000 );
	current_rotation_table.kx   = (vdp2_vram[address/4 + 19] & 0x00ffffff) | ((vdp2_vram[address/4 + 19] & 0x00800000) ? 0xff000000 : 0x00000000 );
	current_rotation_table.ky   = (vdp2_vram[address/4 + 20] & 0x00ffffff) | ((vdp2_vram[address/4 + 20] & 0x00800000) ? 0xff000000 : 0x00000000 );
	current_rotation_table.kast = (vdp2_vram[address/4 + 21] & 0xffffffc0);
	current_rotation_table.dkast= (vdp2_vram[address/4 + 22] & 0x03ffffc0) | ((vdp2_vram[address/4 + 22] & 0x02000000) ? 0xfc000000 : 0x00000000 );
	current_rotation_table.dkax = (vdp2_vram[address/4 + 23] & 0x03ffffc0) | ((vdp2_vram[address/4 + 23] & 0x02000000) ? 0xfc000000 : 0x00000000 );

	/* check rotation parameter read control, override if specific bits are disabled */
	/* (Batman Forever The Riddler stage relies on this) */
	switch(rot_parameter)
	{
		case 1:
			/* TODO: disable read control if these undocumented bits are on (Radiant Silvergun Xiga final boss) */
			if(!VDP2_RAUNK)
			{
				if(!VDP2_RAXSTRE)
					current_rotation_table.xst = 0;

				if(!VDP2_RAYSTRE)
					current_rotation_table.yst = 0;

				if(!VDP2_RAKASTRE)
					current_rotation_table.dkax = 0;
			}
			break;
		case 2:
			/* same as above */
			if(!VDP2_RBUNK)
			{
				if(!VDP2_RBXSTRE)
					current_rotation_table.xst = 0;

				if(!VDP2_RBYSTRE)
					current_rotation_table.yst = 0;

				if(!VDP2_RBKASTRE)
					current_rotation_table.dkax = 0;
			}
			break;
	}

#define RP  current_rotation_table

	log_masked(LOG_ROZ, "Rotation parameter table (%d)\n", rot_parameter);
	log_masked(LOG_ROZ, "xst = %x, yst = %x, zst = %x\n", RP.xst, RP.yst, RP.zst);
	log_masked(LOG_ROZ, "dxst = %x, dyst = %x\n", RP.dxst, RP.dyst);
	log_masked(LOG_ROZ, "dx = %x, dy = %x\n", RP.dx, RP.dy);
	log_masked(LOG_ROZ, "A = %x, B = %x, C = %x, D = %x, E = %x, F = %x\n", RP.A, RP.B, RP.C, RP.D, RP.E, RP.F);
	log_masked(LOG_ROZ, "px = %x, py = %x, pz = %x\n", RP.px, RP.py, RP.pz);
	log_masked(LOG_ROZ, "cx = %x, cy = %x, cz = %x\n", RP.cx, RP.cy, RP.cz);
	log_masked(LOG_ROZ, "mx = %x, my = %x\n", RP.mx, RP.my);
	log_masked(LOG_ROZ, "kx = %x, ky = %x\n", RP.kx, RP.ky);
	log_masked(LOG_ROZ, "kast = %x, dkast = %x, dkax = %x\n", RP.kast, RP.dkast, RP.dkax);

	/*Attempt to show on screen the rotation table*/
	if (DEBUG_DRAW_ROZ)
	{
		if(0)
			m_vdpdebug_roz++;

		if(0)
			m_vdpdebug_roz--;

		if(m_vdpdebug_roz > 10)
			m_vdpdebug_roz = 10;

		switch(m_vdpdebug_roz)
		{
			case 0: popmessage( "Rotation parameter Table (%d)", rot_parameter ); break;
			case 1: popmessage( "xst = %x, yst = %x, zst = %x", RP.xst, RP.yst, RP.zst ); break;
			case 2: popmessage( "dxst = %x, dyst = %x", RP.dxst, RP.dyst ); break;
			case 3: popmessage( "dx = %x, dy = %x", RP.dx, RP.dy ); break;
			case 4: popmessage( "A = %x, B = %x, C = %x, D = %x, E = %x, F = %x", RP.A, RP.B, RP.C, RP.D, RP.E, RP.F ); break;
			case 5: popmessage( "px = %x, py = %x, pz = %x", RP.px, RP.py, RP.pz ); break;
			case 6: popmessage( "cx = %x, cy = %x, cz = %x", RP.cx, RP.cy, RP.cz ); break;
			case 7: popmessage( "mx = %x, my = %x", RP.mx, RP.my ); break;
			case 8: popmessage( "kx = %x, ky = %x", RP.kx, RP.ky ); break;
			case 9: popmessage( "kast = %x, dkast = %x, dkax = %x", RP.kast, RP.dkast, RP.dkax ); break;
			case 10: break;
		}
	}
}

/* check if RGB layer has rotation applied */
static UINT8 vdp2_is_rotation_applied(void)
{
#define _FIXED_1    (0x00010000)
#define _FIXED_0    (0x00000000)

	if ( RP.A == _FIXED_1 &&
			RP.B == _FIXED_0 &&
			RP.C == _FIXED_0 &&
			RP.D == _FIXED_0 &&
			RP.E == _FIXED_1 &&
			RP.F == _FIXED_0 &&
			RP.dxst == _FIXED_0 &&
			RP.dyst == _FIXED_1 &&
			RP.dx == _FIXED_1 &&
			RP.dy == _FIXED_0 &&
			RP.kx == _FIXED_1 &&
			RP.ky == _FIXED_1 &&
			VDP2_RPMD < 2) /* disable optimizations if roz mode is 2 or 3 */
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static UINT8 vdp2_are_map_registers_equal(void)
{
	int i;

	for ( i = 1; i < current_tilemap.map_count; i++ )
	{
		if ( current_tilemap.map_offset[i] != current_tilemap.map_offset[0] )
		{
			return 0;
		}
	}
	return 1;
}

static void vdp2_check_fade_control_for_layer(void)
{
	if ( current_tilemap.fade_control & 1 )
	{
		if ( current_tilemap.fade_control & 2 )
		{
			if ((VDP2_COBR & 0x1ff) == 0 &&
				(VDP2_COBG & 0x1ff) == 0 &&
				(VDP2_COBB & 0x1ff) == 0 )
			{
				current_tilemap.fade_control = 0;
			}
		}
		else
		{
			if ((VDP2_COAR & 0x1ff) == 0 &&
				(VDP2_COAG & 0x1ff) == 0 &&
				(VDP2_COAB & 0x1ff) == 0 )
			{
				current_tilemap.fade_control = 0;
			}
		}
	}
}

#define VDP2_CP_NBG0_PNMDR      0x0
#define VDP2_CP_NBG1_PNMDR      0x1
#define VDP2_CP_NBG2_PNMDR      0x2
#define VDP2_CP_NBG3_PNMDR      0x3
#define VDP2_CP_NBG0_CPDR       0x4
#define VDP2_CP_NBG1_CPDR       0x5
#define VDP2_CP_NBG2_CPDR       0x6
#define VDP2_CP_NBG3_CPDR       0x7

static UINT8 vdp2_check_vram_cycle_pattern_registers( UINT8 access_command_pnmdr, UINT8 access_command_cpdr, UINT8 bitmap_enable )
{
	int i;
	UINT8  access_command_ok = 0;
	UINT16 cp_regs[8];
	cp_regs[0] = VDP2_CYCA0L;
	cp_regs[1] = VDP2_CYCA0U;
	cp_regs[2] = VDP2_CYCA1L;
	cp_regs[3] = VDP2_CYCA1U;
	cp_regs[4] = VDP2_CYCA2L;
	cp_regs[5] = VDP2_CYCA2U;
	cp_regs[6] = VDP2_CYCA3L;
	cp_regs[7] = VDP2_CYCA3U;

	if ( bitmap_enable ) access_command_ok = 1;

	for ( i = 0; i < 8; i++ )
	{
		if ( ((cp_regs[i] >> 12) & 0xf) == access_command_pnmdr )
		{
			access_command_ok |= 1;
		}
		if ( ((cp_regs[i] >> 12) & 0xf) == access_command_cpdr )
		{
			access_command_ok |= 2;
		}
		if ( ((cp_regs[i] >> 8) & 0xf) == access_command_pnmdr )
		{
			access_command_ok |= 1;
		}
		if ( ((cp_regs[i] >> 8) & 0xf) == access_command_cpdr )
		{
			access_command_ok |= 2;
		}
		if ( ((cp_regs[i] >> 4) & 0xf) == access_command_pnmdr )
		{
			access_command_ok |= 1;
		}
		if ( ((cp_regs[i] >> 4) & 0xf) == access_command_cpdr )
		{
			access_command_ok |= 2;
		}
		if ( ((cp_regs[i] >> 0) & 0xf) == access_command_pnmdr )
		{
			access_command_ok |= 1;
		}
		if ( ((cp_regs[i] >> 0) & 0xf) == access_command_cpdr )
		{
			access_command_ok |= 2;
		}
	}
	return access_command_ok == 3 ? 1 : 0;
}


static void vdp2_compute_color_offset( int *r, int *g, int *b, int cor )
{
	if ( cor == 0 )
	{
		*r = (VDP2_COAR & 0x100) ? (*r - (0x100 - (VDP2_COAR & 0xff))) : ((VDP2_COAR & 0xff) + *r);
		*g = (VDP2_COAG & 0x100) ? (*g - (0x100 - (VDP2_COAG & 0xff))) : ((VDP2_COAG & 0xff) + *g);
		*b = (VDP2_COAB & 0x100) ? (*b - (0x100 - (VDP2_COAB & 0xff))) : ((VDP2_COAB & 0xff) + *b);
	}
	else
	{
		*r = (VDP2_COBR & 0x100) ? (*r - (0xff - (VDP2_COBR & 0xff))) : ((VDP2_COBR & 0xff) + *r);
		*g = (VDP2_COBG & 0x100) ? (*g - (0xff - (VDP2_COBG & 0xff))) : ((VDP2_COBG & 0xff) + *g);
		*b = (VDP2_COBB & 0x100) ? (*b - (0xff - (VDP2_COBB & 0xff))) : ((VDP2_COBB & 0xff) + *b);
	}
	if(*r < 0)      { *r = 0; }
	if(*r > 0xff)   { *r = 0xff; }
	if(*g < 0)      { *g = 0; }
	if(*g > 0xff)   { *g = 0xff; }
	if(*b < 0)      { *b = 0; }
	if(*b > 0xff)   { *b = 0xff; }
}

/*
	The color offset of a whole screen at once: the signed offsets of
	set A (cor == 0) or B for the three components, then each pixel only
	adds and clamps.
*/
static void vdp2_color_offset_values(int cor, int *r, int *g, int *b)
{
	if ( cor == 0 )
	{
		*r = (VDP2_COAR & 0x100) ? -(0x100 - (VDP2_COAR & 0xff)) : (VDP2_COAR & 0xff);
		*g = (VDP2_COAG & 0x100) ? -(0x100 - (VDP2_COAG & 0xff)) : (VDP2_COAG & 0xff);
		*b = (VDP2_COAB & 0x100) ? -(0x100 - (VDP2_COAB & 0xff)) : (VDP2_COAB & 0xff);
	}
	else
	{
		*r = (VDP2_COBR & 0x100) ? -(0xff - (VDP2_COBR & 0xff)) : (VDP2_COBR & 0xff);
		*g = (VDP2_COBG & 0x100) ? -(0xff - (VDP2_COBG & 0xff)) : (VDP2_COBG & 0xff);
		*b = (VDP2_COBB & 0x100) ? -(0xff - (VDP2_COBB & 0xff)) : (VDP2_COBB & 0xff);
	}
}

static INLINE rgb_t vdp2_apply_color_offset(rgb_t rgb, int ro, int go, int bo)
{
	int r = (int)RGB_RED(rgb) + ro;
	int g = (int)RGB_GREEN(rgb) + go;
	int b = (int)RGB_BLUE(rgb) + bo;
	if (r < 0) r = 0; else if (r > 0xff) r = 0xff;
	if (g < 0) g = 0; else if (g > 0xff) g = 0xff;
	if (b < 0) b = 0; else if (b > 0xff) b = 0xff;
	return RGB_MAKE(r, g, b);
}

static void vdp2_drawgfxzoom(
		struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx,
		UINT32 code,UINT32 color,int flipx,int flipy,int sx,int sy,
		int transparency,int scalex, int scaley,
		int sprite_screen_width, int sprite_screen_height, int alpha)
{
	int y;
	int x;
	struct rectangle myclip;

	if (!scalex || !scaley) return;

	/* (MAME skips here the tiles its pen usage shows all transparent: the same output) */

	/*
	scalex and scaley are 16.16 fixed point numbers
	1<<15 : shrink to 50%
	1<<16 : uniform scale
	1<<17 : double to 200%
	*/

	/* force clip to bitmap boundary */
	myclip = *clip;
	rect_clip_to_bitmap(&myclip, dest_bmp);

	if (gfx)
	{
		const pen_t *pal = &PEN(0 + gfx->granularity * (color % gfx->colors));
		const UINT8 *source_base = gfx_get_data(gfx, code % gfx->elements);

		/*int sprite_screen_height = (scaley*gfx->height+0x8000)>>16; */
		/*int sprite_screen_width = (scalex*gfx->width+0x8000)>>16; */

		if (sprite_screen_width && sprite_screen_height)
		{
			/* compute sprite increment per screen pixel */
			/*int dx = (gfx->width<<16)/sprite_screen_width; */
			/*int dy = (gfx->height<<16)/sprite_screen_height; */
			int dx = current_tilemap.incx;
			int dy = current_tilemap.incy;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if (flipx)
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if (flipy)
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if (sx < myclip.min_x)
			{
				/* clip left */
				int pixels = myclip.min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if (sy < myclip.min_y )
			{
				/* clip top */
				int pixels = myclip.min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			if (ex > myclip.max_x+1)
			{
				/* clip right */
				int pixels = ex-myclip.max_x-1;
				ex -= pixels;
			}
			if (ey > myclip.max_y+1)
			{
				/* clip bottom */
				int pixels = ey-myclip.max_y-1;
				ey -= pixels;
			}

			/* skip if inner loop doesn't draw anything */
			if (ex > sx )
			{
				if (transparency & STV_TRANSPARENCY_ALPHA)
				{
					/* case : STV_TRANSPARENCY_ALPHA */
					for (y = sy; y < ey; y++)
					{
						UINT8 const *const source = source_base + (y_index>>16) * gfx->width;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for( x=sx; x<ex; x++ )
						{
							if(vdp2_window_process(x, y))
							{
								int c = source[x_index>>16];
								if ((transparency & STV_TRANSPARENCY_NONE) || (c != 0))
									dest[x] = stv_alpha_blend_r32(dest[x], pal[c], alpha);
							}
							x_index += dx;
						}

						y_index += dy;
					}
				}
				else if (transparency & STV_TRANSPARENCY_ADD_BLEND)
				{
					/* case : STV_TRANSPARENCY_ADD_BLEND */
					for (y = sy; y < ey; y++)
					{
						UINT8 const *const source = source_base + (y_index>>16) * gfx->width;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for (x = sx; x < ex; x++)
						{
							if (vdp2_window_process(x, y))
							{
								int c = source[x_index>>16];
								if ((transparency & STV_TRANSPARENCY_NONE) || (c != 0))
									dest[x] = add_blend_r32(dest[x],pal[c]);
							}
							x_index += dx;
						}

						y_index += dy;
					}
				}
				else
				{
					/* case : STV_TRANSPARENCY_PEN */
					for (y = sy; y < ey; y++)
					{
						UINT8 const *const source = source_base + (y_index>>16) * gfx->width;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for (x = sx; x < ex; x++)
						{
							if (vdp2_window_process(x, y))
							{
								int c = source[x_index>>16];
								if ((transparency & STV_TRANSPARENCY_NONE) || (c != 0))
									dest[x] = pal[c];
							}
							x_index += dx;
						}

						y_index += dy;
					}
				}
			}
		}
	}
}

static void vdp2_drawgfxzoom_rgb555(
		struct mame_bitmap *dest_bmp,const struct rectangle *clip,
		UINT32 code,UINT32 color,int flipx,int flipy,int sx,int sy,
		int transparency,int scalex, int scaley,
		int sprite_screen_width, int sprite_screen_height, int alpha)
{
	int y;
	int x;
	struct rectangle myclip;
	UINT8* gfxdata;

	gfxdata = vdp2l.gfx_decode + code * 0x20;

	if(current_tilemap.window_control.enabled[0] ||
		current_tilemap.window_control.enabled[1])
		popmessage("Window Enabled for RGB555 Zoom");

	if (!scalex || !scaley) return;

	#if 0
	if (0 && !(transparency & STV_TRANSPARENCY_NONE))
	{
		int transmask = 0;

		transmask = 1 << (0 & 0xff);

		if ((gfx->pen_usage(code) & ~transmask) == 0)
			/* character is totally transparent, no need to draw */
			return;
		else if ((gfx->pen_usage(code) & transmask) == 0)
			/* character is totally opaque, can disable transparency */
			transparency |= STV_TRANSPARENCY_NONE;
	}
	#endif

	/*
	scalex and scaley are 16.16 fixed point numbers
	1<<15 : shrink to 50%
	1<<16 : uniform scale
	1<<17 : double to 200%
	*/

	/* force clip to bitmap boundary */
	myclip = *clip;
	rect_clip_to_bitmap(&myclip, dest_bmp);

/*  if( gfx ) */
	{
/*      const UINT8 *source_base = gfx_get_data(gfx, code % gfx->elements); */

		/*int sprite_screen_height = (scaley*gfx->height+0x8000)>>16; */
		/*int sprite_screen_width = (scalex*gfx->width+0x8000)>>16; */

		if (sprite_screen_width && sprite_screen_height)
		{
			/* compute sprite increment per screen pixel */
			/*int dx = (gfx->width<<16)/sprite_screen_width; */
			/*int dy = (gfx->height<<16)/sprite_screen_height; */
			int dx = current_tilemap.incx;
			int dy = current_tilemap.incy;

			int ex = sx+sprite_screen_width;
			int ey = sy+sprite_screen_height;

			int x_index_base;
			int y_index;

			if (flipx)
			{
				x_index_base = (sprite_screen_width-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if (flipy)
			{
				y_index = (sprite_screen_height-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if (sx < myclip.min_x)
			{
				/* clip left */
				int pixels = myclip.min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if (sy < myclip.min_y)
			{
				/* clip top */
				int pixels = myclip.min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			if (ex > myclip.max_x+1)
			{
				/* clip right */
				int pixels = ex-myclip.max_x-1;
				ex -= pixels;
			}
			if (ey > myclip.max_y+1)
			{
				/* clip bottom */
				int pixels = ey-myclip.max_y-1;
				ey -= pixels;
			}

			/* skip if inner loop doesn't draw anything */
			if (ex > sx)
			{
				if (transparency & STV_TRANSPARENCY_ALPHA)
				{
					/* case : STV_TRANSPARENCY_ALPHA */
					for(y = sy; y < ey; y++)
					{
						UINT8 const *const source = gfxdata + (y_index>>16)*16;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for (x = sx; x < ex; x++)
						{
							int data = (source[(x_index>>16)*2] << 8) | source[(x_index>>16)*2+1];
							int b = pal5bit((data & 0x7c00) >> 10);
							int g = pal5bit((data & 0x03e0) >> 5);
							int r = pal5bit( data & 0x001f);
							if(current_tilemap.fade_control & 1)
								vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

							if ((transparency & STV_TRANSPARENCY_NONE) || (data & 0x8000))
								dest[x] = stv_alpha_blend_r32(dest[x], RGB_MAKE(r, g, b), alpha);

							x_index += dx;
						}

						y_index += dy;
					}
				}
				else if (transparency & STV_TRANSPARENCY_ADD_BLEND)
				{
					/* case : STV_TRANSPARENCY_ADD_BLEND */
					for (y = sy; y < ey; y++)
					{
						UINT8 const *const source = gfxdata + (y_index>>16)*16;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for (x = sx; x < ex; x++)
						{
							int data = (source[(x_index*2+0)>>16]<<0)|(source[(x_index*2+1)>>16]<<8);
							int b = pal5bit((data & 0x7c00) >> 10);
							int g = pal5bit((data & 0x03e0) >> 5);
							int r = pal5bit( data & 0x001f);
							if(current_tilemap.fade_control & 1)
								vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

							if ((transparency & STV_TRANSPARENCY_NONE) || (data & 0x8000))
								dest[x] = add_blend_r32(dest[x], RGB_MAKE(r, g, b));

							x_index += dx;
						}

						y_index += dy;
					}
				}
				else
				{
					/* case : STV_TRANSPARENCY_PEN */
					for (y = sy; y < ey; y++)
					{
						UINT8 const *const source = gfxdata + (y_index>>16)*16;
						UINT32 *const dest = BMP32(dest_bmp, y);

						int x_index = x_index_base;
						for (x = sx; x < ex; x++)
						{
							int data = (source[(x_index>>16)*2] << 8) | source[(x_index>>16)*2+1];
							int b = pal5bit((data & 0x7c00) >> 10);
							int g = pal5bit((data & 0x03e0) >> 5);
							int r = pal5bit( data & 0x001f);
							if (current_tilemap.fade_control & 1)
								vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

							if ((transparency & STV_TRANSPARENCY_NONE) || (data & 0x8000))
								dest[x] = RGB_MAKE(r, g, b);

							x_index += dx;
						}

						y_index += dy;
					}
				}
			}
		}
	}

}


static void vdp2_drawgfx_rgb555(struct mame_bitmap *dest_bmp, const struct rectangle *clip, UINT32 code, int flipx, int flipy, int sx, int sy, int transparency, int alpha)
{
	int y;
	int x;
	struct rectangle myclip;
	UINT8* gfxdata;
	int sprite_screen_width, sprite_screen_height;

	gfxdata = vdp2l.gfx_decode + code * 0x20;
	sprite_screen_width = sprite_screen_height = 8;

	if(current_tilemap.window_control.enabled[0] ||
		current_tilemap.window_control.enabled[1])
		popmessage("Window Enabled for RGB555 tiles");

	/* force clip to bitmap boundary */
	myclip = *clip;
	rect_clip_to_bitmap(&myclip, dest_bmp);

	{
		int dx = current_tilemap.incx;
		int dy = current_tilemap.incy;

		int ex = sx+sprite_screen_width;
		int ey = sy+sprite_screen_height;

		int x_index_base;
		int y_index;

		if (flipx)
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if (flipy)
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if (sx < myclip.min_x)
		{
			/* clip left */
			int pixels = myclip.min_x-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}
		if (sy < myclip.min_y)
		{
			/* clip top */
			int pixels = myclip.min_y-sy;
			sy += pixels;
			y_index += pixels*dy;
		}
		if (ex > myclip.max_x+1)
		{
			/* clip right */
			int pixels = ex-myclip.max_x-1;
			ex -= pixels;
		}
		if (ey > myclip.max_y+1)
		{
			/* clip bottom */
			int pixels = ey-myclip.max_y-1;
			ey -= pixels;
		}

		/* skip if inner loop doesn't draw anything */
		if (ex > sx)
		{
			for (y = sy; y < ey; y++)
			{
				UINT8 const *const source = gfxdata + (y_index>>16)*16;
				UINT32 *const dest = BMP32(dest_bmp, y);

				int x_index = x_index_base;
				for (x = sx; x < ex; x++)
				{
					UINT16 data = (source[(x_index>>16)*2] << 8) | source[(x_index>>16)*2+1];
					if ((data & 0x8000) || (transparency & STV_TRANSPARENCY_NONE))
					{
						int b = pal5bit((data & 0x7c00) >> 10);
						int g = pal5bit((data & 0x03e0) >> 5);
						int r = pal5bit( data & 0x001f);
						if (current_tilemap.fade_control & 1)
							vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

						if (transparency & STV_TRANSPARENCY_ALPHA)
							dest[x] = stv_alpha_blend_r32( dest[x], RGB_MAKE(r, g, b), alpha );
						else
							dest[x] = RGB_MAKE(r, g, b);
					}
					x_index += dx;
				}

				y_index += dy;
			}

		}

	}

}


static void vdp2_drawgfx_rgb888( struct mame_bitmap *dest_bmp, const struct rectangle *clip, UINT32 code, int flipx, int flipy,
										int sx, int sy, int transparency, int alpha)
{
	int y;
	int x;
	struct rectangle myclip;
	UINT8* gfxdata;
	int sprite_screen_width, sprite_screen_height;

	gfxdata = vdp2l.gfx_decode + code * 0x20;
	sprite_screen_width = sprite_screen_height = 8;

	if(current_tilemap.window_control.enabled[0] ||
		current_tilemap.window_control.enabled[1])
		popmessage("Window Enabled for RGB888 tiles");

	/* force clip to bitmap boundary */
	myclip = *clip;
	rect_clip_to_bitmap(&myclip, dest_bmp);

	{
		int dx = current_tilemap.incx;
		int dy = current_tilemap.incy;

		int ex = sx+sprite_screen_width;
		int ey = sy+sprite_screen_height;

		int x_index_base;
		int y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if( flipy )
		{
			y_index = (sprite_screen_height-1)*dy;
			dy = -dy;
		}
		else
		{
			y_index = 0;
		}

		if( sx < myclip.min_x)
		{
			/* clip left */
			int pixels = myclip.min_x-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}
		if( sy < myclip.min_y )
		{
			/* clip top */
			int pixels = myclip.min_y-sy;
			sy += pixels;
			y_index += pixels*dy;
		}
		if( ex > myclip.max_x+1 )
		{
			/* clip right */
			int pixels = ex-myclip.max_x-1;
			ex -= pixels;
		}
		if( ey > myclip.max_y+1 )
		{
			/* clip bottom */
			int pixels = ey-myclip.max_y-1;
			ey -= pixels;
		}

		/* skip if inner loop doesn't draw anything */
		if( ex > sx )
		{
			for( y=sy; y<ey; y++ )
			{
				UINT8 const *const source = gfxdata + (y_index>>16)*32;
				UINT32 *const dest = BMP32(dest_bmp, y);

				int x_index = x_index_base;

				for( x=sx; x<ex; x++ )
				{
					UINT32 data = (source[(x_index>>16)*4+0] << 24) | (source[(x_index>>16)*4+1] << 16) | (source[(x_index>>16)*4+2] << 8) | (source[(x_index>>16)*4+3] << 0);
					if ((data & 0x80000000) || (transparency & STV_TRANSPARENCY_NONE))
					{
						int b = (data & 0xff0000) >> 16;
						int g = (data & 0x00ff00) >> 8;
						int r = (data & 0x0000ff);

						if(current_tilemap.fade_control & 1)
							vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

						if (transparency & STV_TRANSPARENCY_ALPHA)
							dest[x] = stv_alpha_blend_r32( dest[x], RGB_MAKE(r, g, b), alpha );
						else
							dest[x] = RGB_MAKE(r, g, b);
					}
					x_index += dx;
				}

				y_index += dy;
			}

		}

	}
}

static void vdp2_drawgfx_alpha(struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx,
							UINT32 code,UINT32 color, int flipx,int flipy,int offsx,int offsy,
							int transparency, int alpha)
{
	int y;
	int x;
	const pen_t *pal = &PEN(0 + gfx->granularity * (color % gfx->colors));
	const UINT8 *source_base = gfx_get_data(gfx, code % gfx->elements);
	int x_index_base, y_index, sx, sy, ex, ey;
	int xinc, yinc;

	xinc = flipx ? -1 : 1;
	yinc = flipy ? -1 : 1;

	x_index_base = flipx ? gfx->width-1 : 0;
	y_index = flipy ? gfx->height-1 : 0;

	/* start coordinates */
	sx = offsx;
	sy = offsy;

	/* end coordinates */
	ex = sx + gfx->width;
	ey = sy + gfx->height;

	if (sx < clip->min_x)
	{
		/* clip left */
		int pixels = clip->min_x-sx;
		sx += pixels;
		x_index_base += xinc*pixels;
	}
	if (sy < clip->min_y)
	{
		/* clip top */
		int pixels = clip->min_y-sy;
		sy += pixels;
		y_index += yinc*pixels;
	}
	if (ex > clip->max_x+1)
	{
		/* clip right */
		ex = clip->max_x+1;
	}
	if (ey > clip->max_y+1)
	{
		/* clip bottom */
		ey = clip->max_y+1;
	}

	/* skip if inner loop doesn't draw anything */
	if (ex > sx)
	{
		for (y = sy; y < ey; y++)
		{
			UINT8 const *const source = source_base + y_index*gfx->width;
			UINT32 *const dest = BMP32(dest_bmp, y);
			int x_index = x_index_base;
			for (x = sx; x < ex; x++)
			{
				if(vdp2_window_process(x,y))
				{
					int c = (source[x_index]);
					if ((transparency & STV_TRANSPARENCY_NONE) || (c != 0))
						dest[x] = stv_alpha_blend_r32( dest[x], pal[c], alpha );
				}

				x_index += xinc;
			}
			y_index += yinc;
		}
	}
}

/* the windows of a line for the current layer settings, refreshed when they change */
static VDP2_TLS struct
{
	int gen, y, key;
	int s0, e0, in0;	/* window 0: x range, line inside */
	int s1, e1, in1;	/* window 1 */
} vdp2_wline;
static void vdp2_window_line(int y, int key);
static INLINE void vdp2_get_window0_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y);
static INLINE void vdp2_get_window1_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y);

/* the runs of pixels a line shows through the windows of the current layer
   (vdp2_window_process gives 1 inside them), one entry per line, for the
   settings in 'key': the windows only change at s0, e0 + 1, s1 and e1 + 1.
   Not thread local, as vdp2_window_coords */
#define VDP2_SPAN_MIN	(-0x8000)
#define VDP2_SPAN_MAX	0x8000
static struct vdp2_window_span { int gen, key, n; int a[5], b[5]; } vdp2_window_spans[VDP2_WINDOW_LINES];

static const struct vdp2_window_span *vdp2_window_spans_get(int y)
{
	static struct vdp2_window_span outside;	/* lines past the cache: not cached */
	const int en0 = current_tilemap.window_control.enabled[0];
	const int en1 = current_tilemap.window_control.enabled[1];
	const int out0 = !current_tilemap.window_control.area[0];
	const int out1 = !current_tilemap.window_control.area[1];
	const int logic_or = current_tilemap.window_control.logic & 1;
	const int key = (en0 ? 1 : 0) | (en1 ? 2 : 0) | (out0 ? 4 : 0) | (out1 ? 8 : 0) | (logic_or ? 16 : 0);
	struct vdp2_window_span *sp;
	int s0 = 0, e0 = 0, in0 = 0, s1 = 0, e1 = 0, in1 = 0;
	int bp[6], nb = 0, i, j;

	if (y >= 0 && y < VDP2_WINDOW_LINES)
	{
		sp = &vdp2_window_spans[y];
		if (sp->gen == vdp2_window_cache_gen && sp->key == key)
			return sp;
	}
	else
		sp = &outside;

	if (en0)
	{
		int wsy, wey;
		vdp2_get_window0_coordinates(&s0, &e0, &wsy, &wey, y);
		in0 = (y >= wsy && y <= wey);
	}
	if (en1)
	{
		int wsy, wey;
		vdp2_get_window1_coordinates(&s1, &e1, &wsy, &wey, y);
		in1 = (y >= wsy && y <= wey);
	}

	bp[nb++] = VDP2_SPAN_MIN;
	if (en0)
	{
		if (s0 > VDP2_SPAN_MIN && s0 < VDP2_SPAN_MAX) bp[nb++] = s0;
		if (e0 + 1 > VDP2_SPAN_MIN && e0 + 1 < VDP2_SPAN_MAX) bp[nb++] = e0 + 1;
	}
	if (en1)
	{
		if (s1 > VDP2_SPAN_MIN && s1 < VDP2_SPAN_MAX) bp[nb++] = s1;
		if (e1 + 1 > VDP2_SPAN_MIN && e1 + 1 < VDP2_SPAN_MAX) bp[nb++] = e1 + 1;
	}
	for (i = 2; i < nb; i++)
		for (j = i; j > 1 && bp[j - 1] > bp[j]; j--)
		{
			int k = bp[j]; bp[j] = bp[j - 1]; bp[j - 1] = k;
		}
	bp[nb++] = VDP2_SPAN_MAX;

	sp->n = 0;
	for (i = 0; i + 1 < nb; i++)
	{
		int a = bp[i], b = bp[i + 1], w0 = 1, w1 = 1;
		if (a >= b)
			continue;
		if (en0)
			w0 = (in0 && a >= s0 && a <= e0) ^ out0;
		if (en1)
			w1 = (in1 && a >= s1 && a <= e1) ^ out1;
		if (!(logic_or ? (w0 | w1) : (w0 & w1)))
			continue;
		/* a run next to the previous one is joined to it */
		if (sp->n && sp->b[sp->n - 1] == a)
			sp->b[sp->n - 1] = b;
		else
		{
			sp->a[sp->n] = a;
			sp->b[sp->n] = b;
			sp->n++;
		}
	}
	sp->gen = vdp2_window_cache_gen;
	sp->key = key;
	return sp;
}

static void vdp2_drawgfx_transpen(struct mame_bitmap *dest_bmp,const struct rectangle *clip,gfx_element *gfx,
							UINT32 code,UINT32 color, int flipx,int flipy,int offsx,int offsy,
							int transparency)
{
	int y;
	int x;
	const pen_t *pal = &PEN(0 + gfx->granularity * (color % gfx->colors));
	const UINT8 *source_base = gfx_get_data(gfx, code % gfx->elements);
	int x_index_base, y_index, sx, sy, ex, ey;
	int xinc, yinc;

	xinc = flipx ? -1 : 1;
	yinc = flipy ? -1 : 1;

	x_index_base = flipx ? gfx->width-1 : 0;
	y_index = flipy ? gfx->height-1 : 0;

	/* start coordinates */
	sx = offsx;
	sy = offsy;

	/* end coordinates */
	ex = sx + gfx->width;
	ey = sy + gfx->height;

	if (sx < clip->min_x)
	{
		/* clip left */
		int pixels = clip->min_x-sx;
		sx += pixels;
		x_index_base += xinc*pixels;
	}
	if (sy < clip->min_y)
	{
		/* clip top */
		int pixels = clip->min_y-sy;
		sy += pixels;
		y_index += yinc*pixels;
	}
	if (ex > clip->max_x+1)
	{
		/* clip right */
		ex = clip->max_x+1;
	}
	if (ey > clip->max_y+1)
	{
		/* clip bottom */
		ey = clip->max_y+1;
	}

	/* skip if inner loop doesn't draw anything */
	if (ex <= sx)
		return;

	/* no window on this layer: plain copy loops (the RBG0 cache is drawn this way) */
	if (!current_tilemap.window_control.enabled[0] && !current_tilemap.window_control.enabled[1])
	{
		const int opaque = (transparency & STV_TRANSPARENCY_NONE) ? 1 : 0;
		const int width = ex - sx;

		for (y = sy; y < ey; y++)
		{
			UINT8 const *source = source_base + y_index*gfx->width + x_index_base;
			UINT32 *dest = BMP32(dest_bmp, y) + sx;
			int n;

			if (opaque)
			{
				for (n = 0; n < width; n++, source += xinc)
					dest[n] = pal[*source];
			}
			else if (xinc == 1)
			{
				/* four pixels at a time: a group of transparent ones is skipped */
				for (n = 0; n + 4 <= width; n += 4)
				{
					UINT32 four;
					int c;
					memcpy(&four, source + n, 4);
					if (four == 0)
						continue;
					c = source[n];     if (c != 0) dest[n] = pal[c];
					c = source[n + 1]; if (c != 0) dest[n + 1] = pal[c];
					c = source[n + 2]; if (c != 0) dest[n + 2] = pal[c];
					c = source[n + 3]; if (c != 0) dest[n + 3] = pal[c];
				}
				for (; n < width; n++)
				{
					const int c = source[n];
					if (c != 0)
						dest[n] = pal[c];
				}
			}
			else
			{
				for (n = 0; n < width; n++, source += xinc)
				{
					const int c = *source;
					if (c != 0)
						dest[n] = pal[c];
				}
			}
			y_index += yinc;
		}
		return;
	}

	/* vdp2_window_process: the runs of pixels a line shows, from the cache of
	   the line (vdp2_window_spans) */
	{
		const int opaque = (transparency & STV_TRANSPARENCY_NONE) ? 1 : 0;

		for (y = sy; y < ey; y++)
		{
			UINT8 const *const source = source_base + y_index*gfx->width;
			UINT32 *const dest = BMP32(dest_bmp, y);
			const struct vdp2_window_span *sp = vdp2_window_spans_get(y);
			int i;

			for (i = 0; i < sp->n; i++)
			{
				int a = sp->a[i], b = sp->b[i], x_index;
				if (a < sx) a = sx;
				if (b > ex) b = ex;
				if (a >= b)
					continue;
				x_index = x_index_base + (a - sx) * xinc;
				if (opaque)
				{
					for (x = a; x < b; x++, x_index += xinc)
						dest[x] = pal[source[x_index]];
				}
				else if (xinc == 1)
				{
					/* four pixels at a time: a group of transparent ones is skipped */
					const UINT8 *s = source + x_index;
					UINT32 *d = dest + a;
					int n, count = b - a;
					for (n = 0; n + 4 <= count; n += 4)
					{
						UINT32 four;
						int c;
						memcpy(&four, s + n, 4);
						if (four == 0)
							continue;
						c = s[n];     if (c != 0) d[n] = pal[c];
						c = s[n + 1]; if (c != 0) d[n + 1] = pal[c];
						c = s[n + 2]; if (c != 0) d[n + 2] = pal[c];
						c = s[n + 3]; if (c != 0) d[n + 3] = pal[c];
					}
					for (; n < count; n++)
					{
						const int c = s[n];
						if (c != 0)
							d[n] = pal[c];
					}
				}
				else
				{
					for (x = a; x < b; x++, x_index += xinc)
					{
						const int c = source[x_index];
						if (c != 0)
							dest[x] = pal[c];
					}
				}
			}
			y_index += yinc;
		}
	}
}

/* - arcadegh uses incy zoom for most games but Joust */
static void draw_4bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int xsize, ysize, xsize_mask, ysize_mask;
	int xsrc,ysrc,xdst,ydst;
	int src_offs;
	UINT8* vram = vdp2l.gfx_decode;
	UINT32 map_offset = current_tilemap.bitmap_map * 0x20000;
	int scrollx = current_tilemap.scrollx;
	int scrolly = current_tilemap.scrolly;
	UINT16 dot_data;
	UINT16 pal_bank;
	int xf, yf;

	xsize = (current_tilemap.bitmap_size & 2) ? 1024 : 512;
	ysize = (current_tilemap.bitmap_size & 1) ? 512 : 256;

	xsize_mask = (current_tilemap.linescroll_enable) ? 1024 : xsize;
	ysize_mask = (current_tilemap.vertical_linescroll_enable) ? 512 : ysize;

	pal_bank = current_tilemap.bitmap_palette_number;
	pal_bank+= current_tilemap.colour_ram_address_offset;
	pal_bank&= 7;
	pal_bank<<=8;
	if(current_tilemap.fade_control & 1)
		pal_bank += ((current_tilemap.fade_control & 2) ? (2*2048) : (2048));

	for(ydst=cliprect->min_y;ydst<=cliprect->max_y;ydst++)
	{
		for(xdst=cliprect->min_x;xdst<=cliprect->max_x;xdst++)
		{
			if(!vdp2_window_process(xdst,ydst))
				continue;

			xf = current_tilemap.incx * xdst;
			xf>>=16;
			yf = current_tilemap.incy * ydst;
			yf>>=16;

			xsrc = (xf + scrollx) & (xsize_mask-1);
			ysrc = (yf + scrolly) & (ysize_mask-1);
			src_offs = (xsrc + (ysrc*xsize));
			src_offs/= 2;
			src_offs += map_offset;
			src_offs &= 0x7ffff;

			dot_data = vram[src_offs] >> ((xsrc & 1) ? 0 : 4);
			dot_data&= 0xf;

			if ((dot_data != 0) || (current_tilemap.transparency & STV_TRANSPARENCY_NONE))
			{
				dot_data += pal_bank;

				if ( current_tilemap.colour_calculation_enabled == 0 )
					BMP32(bitmap, ydst)[xdst] = PEN(dot_data);
				else
					BMP32(bitmap, ydst)[xdst] = stv_alpha_blend_r32(BMP32(bitmap, ydst)[xdst], PEN(dot_data), current_tilemap.alpha);
			}
		}
	}
}


static void draw_8bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int xsize, ysize, xsize_mask, ysize_mask;
	int xsrc,ysrc,xdst,ydst;
	int src_offs;
	UINT8* vram = vdp2l.gfx_decode;
	UINT32 map_offset = current_tilemap.bitmap_map * 0x20000;
	int scrollx = current_tilemap.scrollx;
	int scrolly = current_tilemap.scrolly;
	UINT16 dot_data;
	UINT16 pal_bank;
	int xf, yf;

	xsize = (current_tilemap.bitmap_size & 2) ? 1024 : 512;
	ysize = (current_tilemap.bitmap_size & 1) ? 512 : 256;

	xsize_mask = (current_tilemap.linescroll_enable) ? 1024 : xsize;
	ysize_mask = (current_tilemap.vertical_linescroll_enable) ? 512 : ysize;

	pal_bank = current_tilemap.bitmap_palette_number;
	pal_bank+= current_tilemap.colour_ram_address_offset;
	pal_bank&= 7;
	pal_bank<<=8;
	if(current_tilemap.fade_control & 1)
		pal_bank += ((current_tilemap.fade_control & 2) ? (2*2048) : (2048));

	for(ydst=cliprect->min_y;ydst<=cliprect->max_y;ydst++)
	{
		for(xdst=cliprect->min_x;xdst<=cliprect->max_x;xdst++)
		{
			if(!vdp2_window_process(xdst,ydst))
				continue;

			xf = current_tilemap.incx * xdst;
			xf>>=16;
			yf = current_tilemap.incy * ydst;
			yf>>=16;

			xsrc = (xf + scrollx) & (xsize_mask-1);
			ysrc = (yf + scrolly) & (ysize_mask-1);
			src_offs = (xsrc + (ysrc*xsize));
			src_offs += map_offset;
			src_offs &= 0x7ffff;

			dot_data = vram[src_offs];

			if ((dot_data != 0) || (current_tilemap.transparency & STV_TRANSPARENCY_NONE))
			{
				dot_data += pal_bank;

				if ( current_tilemap.colour_calculation_enabled == 0 )
					BMP32(bitmap, ydst)[xdst] = PEN(dot_data);
				else
					BMP32(bitmap, ydst)[xdst] = stv_alpha_blend_r32(BMP32(bitmap, ydst)[xdst], PEN(dot_data), current_tilemap.alpha);
			}
		}
	}
}

static void draw_11bpp_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int xsize, ysize, xsize_mask, ysize_mask;
	int xsrc,ysrc,xdst,ydst;
	int src_offs;
	UINT8* vram = vdp2l.gfx_decode;
	UINT32 map_offset = current_tilemap.bitmap_map * 0x20000;
	int scrollx = current_tilemap.scrollx;
	int scrolly = current_tilemap.scrolly;
	UINT16 dot_data;
	UINT16 pal_bank;
	int xf, yf;

	xsize = (current_tilemap.bitmap_size & 2) ? 1024 : 512;
	ysize = (current_tilemap.bitmap_size & 1) ? 512 : 256;

	xsize_mask = (current_tilemap.linescroll_enable) ? 1024 : xsize;
	ysize_mask = (current_tilemap.vertical_linescroll_enable) ? 512 : ysize;

	pal_bank = 0;
	if(current_tilemap.fade_control & 1)
		pal_bank = ((current_tilemap.fade_control & 2) ? (2*2048) : (2048));

	for(ydst=cliprect->min_y;ydst<=cliprect->max_y;ydst++)
	{
		for(xdst=cliprect->min_x;xdst<=cliprect->max_x;xdst++)
		{
			if(!vdp2_window_process(xdst,ydst))
				continue;

			xf = current_tilemap.incx * xdst;
			xf>>=16;
			yf = current_tilemap.incy * ydst;
			yf>>=16;

			xsrc = (xf + scrollx) & (xsize_mask-1);
			ysrc = (yf + scrolly) & (ysize_mask-1);
			src_offs = (xsrc + (ysrc*xsize));
			src_offs *= 2;
			src_offs += map_offset;
			src_offs &= 0x7ffff;

			dot_data = ((vram[src_offs]<<8)|(vram[src_offs+1]<<0)) & 0x7ff;

			if ((dot_data != 0) || (current_tilemap.transparency & STV_TRANSPARENCY_NONE))
			{
				dot_data += pal_bank;

				if ( current_tilemap.colour_calculation_enabled == 0 )
					BMP32(bitmap, ydst)[xdst] = PEN(dot_data);
				else
					BMP32(bitmap, ydst)[xdst] = stv_alpha_blend_r32(BMP32(bitmap, ydst)[xdst], PEN(dot_data), current_tilemap.alpha);
			}
		}
	}
}


static void draw_rgb15_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int xsize, ysize, xsize_mask, ysize_mask;
	int xsrc,ysrc,xdst,ydst;
	int src_offs;
	UINT8* vram = vdp2l.gfx_decode;
	UINT32 map_offset = current_tilemap.bitmap_map * 0x20000;
	int scrollx = current_tilemap.scrollx;
	int scrolly = current_tilemap.scrolly;
	int r,g,b;
	UINT16 dot_data;
	int xf, yf;

	xsize = (current_tilemap.bitmap_size & 2) ? 1024 : 512;
	ysize = (current_tilemap.bitmap_size & 1) ? 512 : 256;

	xsize_mask = (current_tilemap.linescroll_enable) ? 1024 : xsize;
	ysize_mask = (current_tilemap.vertical_linescroll_enable) ? 512 : ysize;

	for(ydst=cliprect->min_y;ydst<=cliprect->max_y;ydst++)
	{
		for(xdst=cliprect->min_x;xdst<=cliprect->max_x;xdst++)
		{
			if(!vdp2_window_process(xdst,ydst))
				continue;

			xf = current_tilemap.incx * xdst;
			xf>>=16;
			yf = current_tilemap.incy * ydst;
			yf>>=16;

			xsrc = (xf + scrollx) & (xsize_mask-1);
			ysrc = (yf + scrolly) & (ysize_mask-1);
			src_offs = (xsrc + (ysrc*xsize));
			src_offs *= 2;
			src_offs += map_offset;
			src_offs &= 0x7ffff;

			dot_data =(vram[src_offs]<<8)|(vram[src_offs+1]<<0);

			if ((dot_data & 0x8000) || (current_tilemap.transparency & STV_TRANSPARENCY_NONE))
			{
				b = pal5bit((dot_data & 0x7c00) >> 10);
				g = pal5bit((dot_data & 0x03e0) >> 5);
				r = pal5bit((dot_data & 0x001f) >> 0);

				if(current_tilemap.fade_control & 1)
					vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

				if ( current_tilemap.colour_calculation_enabled == 0 )
					BMP32(bitmap, ydst)[xdst] = RGB_MAKE(r, g, b);
				else
					BMP32(bitmap, ydst)[xdst] = stv_alpha_blend_r32( BMP32(bitmap, ydst)[xdst], RGB_MAKE(r, g, b), current_tilemap.alpha );
			}
		}
	}
}

static void draw_rgb32_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int xsize, ysize, xsize_mask, ysize_mask;
	int xsrc,ysrc,xdst,ydst;
	int src_offs;
	UINT8* vram = vdp2l.gfx_decode;
	UINT32 map_offset = current_tilemap.bitmap_map * 0x20000;
	int scrollx = current_tilemap.scrollx;
	int scrolly = current_tilemap.scrolly;
	int r,g,b;
	UINT32 dot_data;
	int xf, yf;

	xsize = (current_tilemap.bitmap_size & 2) ? 1024 : 512;
	ysize = (current_tilemap.bitmap_size & 1) ? 512 : 256;

	xsize_mask = (current_tilemap.linescroll_enable) ? 1024 : xsize;
	ysize_mask = (current_tilemap.vertical_linescroll_enable) ? 512 : ysize;

	for(ydst=cliprect->min_y;ydst<=cliprect->max_y;ydst++)
	{
		for(xdst=cliprect->min_x;xdst<=cliprect->max_x;xdst++)
		{
			if(!vdp2_window_process(xdst,ydst))
				continue;

			xf = current_tilemap.incx * xdst;
			xf>>=16;
			yf = current_tilemap.incy * ydst;
			yf>>=16;

			xsrc = (xf + scrollx) & (xsize_mask-1);
			ysrc = (yf + scrolly) & (ysize_mask-1);
			src_offs = (xsrc + (ysrc*xsize));
			src_offs *= 4;
			src_offs += map_offset;
			src_offs &= 0x7ffff;

			dot_data = (vram[src_offs+0]<<24)|(vram[src_offs+1]<<16)|(vram[src_offs+2]<<8)|(vram[src_offs+3]<<0);

			if ((dot_data & 0x80000000) || (current_tilemap.transparency & STV_TRANSPARENCY_NONE))
			{
				b = ((dot_data & 0x00ff0000) >> 16);
				g = ((dot_data & 0x0000ff00) >> 8);
				r = ((dot_data & 0x000000ff) >> 0);

				if(current_tilemap.fade_control & 1)
					vdp2_compute_color_offset(&r,&g,&b,current_tilemap.fade_control & 2);

				if ( current_tilemap.colour_calculation_enabled == 0 )
					BMP32(bitmap, ydst)[xdst] = RGB_MAKE(r, g, b);
				else
					BMP32(bitmap, ydst)[xdst] = stv_alpha_blend_r32( BMP32(bitmap, ydst)[xdst], RGB_MAKE(r, g, b), current_tilemap.alpha );
			}
		}
	}
}


static void vdp2_draw_basic_bitmap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	if (!current_tilemap.enabled) return;

	/* new bitmap code, supposed to rewrite the old one. Not supposed to be clean, but EFFICIENT! */
	if(current_tilemap.incx == 0x10000 && current_tilemap.incy == 0x10000)
	{
		switch(current_tilemap.colour_depth)
		{
			case 0: draw_4bpp_bitmap(bitmap,cliprect); return;
			case 1: draw_8bpp_bitmap(bitmap,cliprect); return;
			case 2: draw_11bpp_bitmap(bitmap, cliprect); return;
			case 3: draw_rgb15_bitmap(bitmap,cliprect); return;
			case 4: draw_rgb32_bitmap(bitmap,cliprect); return;
		}

		/* intentional fall-through*/
		popmessage("%d %s %s %s",current_tilemap.colour_depth,
									current_tilemap.transparency & STV_TRANSPARENCY_NONE ? "no trans" : "trans",
									current_tilemap.colour_calculation_enabled ? "cc" : "no cc",
									(current_tilemap.incx == 0x10000 && current_tilemap.incy == 0x10000) ? "no zoom" : "zoom");
	}
	else
	{
		switch(current_tilemap.colour_depth)
		{
			case 0: draw_4bpp_bitmap(bitmap,cliprect); return;
			case 1: draw_8bpp_bitmap(bitmap,cliprect); return;
		/*  case 2: draw_11bpp_bitmap(bitmap,cliprect); return; */
			case 3: draw_rgb15_bitmap(bitmap,cliprect); return;
			case 4: draw_rgb32_bitmap(bitmap,cliprect); return;
		}

		/* intentional fall-through*/
		popmessage("%d %s %s %s",current_tilemap.colour_depth,
									current_tilemap.transparency & STV_TRANSPARENCY_NONE ? "no trans" : "trans",
									current_tilemap.colour_calculation_enabled ? "cc" : "no cc",
									(current_tilemap.incx == 0x10000 && current_tilemap.incy == 0x10000) ? "no zoom" : "zoom");
	}
}

	/*---------------------------------------------------------------------------
	| Plane Size | Pattern Name Data Size | Character Size | Map Bits / Address |
	----------------------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 6-0 * 0x02000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-0 * 0x00800 |
	| 1 H x 1 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-0 * 0x04000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-0 * 0x01000 |
	-----------------------------------------------------------------------------
	|            |                        | 1 H x 1 V      | bits 6-1 * 0x04000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-1 * 0x01000 |
	| 2 H x 1 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-1 * 0x08000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-1 * 0x02000 |
	-----------------------------------------------------------------------------
	|            |                        | 1 H x 1 V      | bits 6-2 * 0x08000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-2 * 0x02000 |
	| 2 H x 2 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-2 * 0x10000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-2 * 0x04000 |
	--the-highest-bit-is-ignored-if-vram-is-only-4mbits------------------------*/


/*
4.2 Sega's Cell / Character Pattern / Page / Plane / Map system, aka a rather annoying thing that makes optimizations hard
 (this is only for the normal tilemaps at the moment, i haven't even thought about the ROZ ones)

Tiles:

Cells are 8x8 gfx stored in video ram, they can be of various colour depths

Character Patterns can be 8x8 or 16x16 (1 hcell x 1 vcell or 2 hcell x 2 vcell)
  (a 16x16 character pattern is 4 8x8 cells put together)

A page is made up of 64x64 cells, thats 64x64 character patterns in 8x8 mode or 32x32 character patterns in 16x16 mode.
  64 * 8  = 512 (0x200)
  32 * 16 = 512 (0x200)
A page is _always_ 512 (0x200) pixels in each direction

in 1 word mode a 32*16 x 32*16 page is 0x0800 bytes
in 1 word mode a 64*8  x 64*8  page is 0x2000 bytes
in 2 word mode a 32*16 x 32*16 page is 0x1000 bytes
in 2 word mode a 64*8  x 64*8  page is 0x4000 bytes

either 1, 2 or 4 pages make each plane depending on the plane size register (per tilemap)
  therefore each plane is either
  64 * 8 * 1 x 64 * 8 * 1 (512 x 512)
  64 * 8 * 2 x 64 * 8 * 1 (1024 x 512)
  64 * 8 * 2 x 64 * 8 * 2 (1024 x 1024)

  32 * 16 * 1 x 32 * 16 * 1 (512 x 512)
  32 * 16 * 2 x 32 * 16 * 1 (1024 x 512)
  32 * 16 * 2 x 32 * 16 * 2 (1024 x 1024)

map is always enabled?
  map is a 2x2 arrangement of planes, all 4 of the planes can be the same.

*/

static void vdp2_get_map_page( int x, int y, int *_map, int *_page )
{
	int page = 0;
	int map;

	if ( current_tilemap.map_count == 4 )
	{
		if ( current_tilemap.tile_size == 0 )
		{
			if ( current_tilemap.plane_size & 1 )
			{
				page = ((x >> 6) & 1);
				map = (x >> 7) & 1;
			}
			else
			{
				map = (x >> 6) & 1;
			}

			if ( current_tilemap.plane_size & 2 )
			{
				page |= ((y >> (6-1)) & 2);
				map |= ((y >> (7-1)) & 2);
			}
			else
			{
				map |= ((y >> (6-1)) & 2);
			}
		}
		else
		{
			if ( current_tilemap.plane_size & 1 )
			{
				page = ((x >> 5) & 1);
				map = (x >> 6) & 1;
			}
			else
			{
				map = (x >> 5) & 1;
			}

			if ( current_tilemap.plane_size & 2 )
			{
				page |= ((y >> (5 - 1)) & 2);
				map |= ((y >> (6-1)) & 2);
			}
			else
			{
				map |= ((y >> (5-1)) & 2);
			}
		}
	}
	else /*16 */
	{
		if ( current_tilemap.tile_size == 0 )
		{
			if ( current_tilemap.plane_size & 1 )
			{
				page = ((x >> 6) & 1);
				map = (x >> 7) & 3;
			}
			else
			{
				map = (x >> 6) & 3;
			}

			if ( current_tilemap.plane_size & 2 )
			{
				page |= ((y >> (6-1)) & 2);
				map |= ((y >> (7-2)) & 12);
			}
			else
			{
				map |= ((y >> (6-2)) & 12);
			}
		}
		else
		{
			if ( current_tilemap.plane_size & 1 )
			{
				page = ((x >> 5) & 1);
				map = (x >> 6) & 3;
			}
			else
			{
				map = (x >> 5) & 3;
			}

			if ( current_tilemap.plane_size & 2 )
			{
				page |= ((y >> (5 - 1)) & 2);
				map |= ((y >> (6-2)) & 12);
			}
			else
			{
				map |= ((y >> (5-2)) & 12);
			}
		}
	}
	*_page = page;
	*_map = map;
}

static void vdp2_draw_basic_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/* hopefully this is easier to follow than it is efficient .. */

	/* I call character patterns tiles .. even if they represent up to 4 tiles */

	/* Page variables */
	int pgtiles_x, pgpixels_x;
	int pgtiles_y, pgpixels_y;
	int pgsize_bytes, pgsize_dwords;

	/* Plane Variables */
	int pltiles_x, plpixels_x;
	int pltiles_y, plpixels_y;
	int plsize_bytes/*, plsize_dwords*/;

	/* Map Variables */
	int mptiles_x, mppixels_x;
	int mptiles_y, mppixels_y;
	int mpsize_bytes, mpsize_dwords;

	/* work Variables */
	int i, x, y;
	int base[16];

	int scalex,scaley;
	int tilesizex, tilesizey;
	int drawypos, drawxpos;

	int tilecodemin = 0x10000000, tilecodemax = 0;
	int palmin = 0x10000000, palmax = 0;

	if ( current_tilemap.incx == 0 || current_tilemap.incy == 0 ) return;

	if ( current_tilemap.colour_calculation_enabled == 1 )
	{
		if ( VDP2_CCMD )
		{
			current_tilemap.transparency |= STV_TRANSPARENCY_ADD_BLEND;
		}
		else
		{
			current_tilemap.transparency |= STV_TRANSPARENCY_ALPHA;
		}
	}

	scalex = (INT32)(((INT64)1 << 32) / (INT64)current_tilemap.incx);
	scaley = (INT32)(((INT64)1 << 32) / (INT64)current_tilemap.incy);
	tilesizex = scalex * 8;
	tilesizey = scaley * 8;
	drawypos = drawxpos = 0;

	/* Calculate the Number of tiles for x / y directions of each page (actually these will be the same */
	/* (2-current_tilemap.tile_size) << 5) */
	pgtiles_x = ((2-current_tilemap.tile_size) << 5); /* 64 (8x8 mode) or 32 (16x16 mode) */
	pgtiles_y = ((2-current_tilemap.tile_size) << 5); /* 64 (8x8 mode) or 32 (16x16 mode) */

	/* Calculate the Page Size in BYTES */
	/* 64 * 64 * (1 * 2) = 0x2000 bytes
	   32 * 32 * (1 * 2) = 0x0800 bytes
	   64 * 64 * (2 * 2) = 0x4000 bytes
	   32 * 32 * (2 * 2) = 0x1000 bytes */

	pgsize_bytes = (pgtiles_x * pgtiles_y) * ((2-current_tilemap.pattern_data_size)*2);

	/*---------------------------------------------------------------------------
	| Plane Size | Pattern Name Data Size | Character Size | Map Bits / Address |
	----------------------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 6-0 * 0x02000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-0 * 0x00800 |
	| 1 H x 1 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-0 * 0x04000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-0 * 0x01000 |
	---------------------------------------------------------------------------*/


	/* Page Dimensions are always 0x200 pixes (512x512) */
	pgpixels_x = 0x200;
	pgpixels_y = 0x200;

	/* Work out the Plane Size in tiles and Plane Dimensions (pixels) */
	switch (current_tilemap.plane_size & 3)
	{
		case 0: /* 1 page * 1 page */
			pltiles_x  = pgtiles_x;
			plpixels_x = pgpixels_x;
			pltiles_y  = pgtiles_y;
			plpixels_y = pgpixels_y;
			break;

		case 1: /* 2 pages * 1 page */
			pltiles_x  = pgtiles_x * 2;
			plpixels_x = pgpixels_x * 2;
			pltiles_y  = pgtiles_y;
			plpixels_y = pgpixels_y;
			break;

		case 3: /* 2 pages * 2 pages */
			pltiles_x  = pgtiles_x * 2;
			plpixels_x = pgpixels_x * 2;
			pltiles_y  = pgtiles_y * 2;
			plpixels_y = pgpixels_y * 2;
			break;

		default:
			/* illegal */
			pltiles_x  = pgtiles_x;
			plpixels_x = pgpixels_x;
			pltiles_y  = pgtiles_y * 2;
			plpixels_y = pgpixels_y * 2;
		break;
	}

	/* Plane Size in BYTES */
	/* still the same as before
	   (64 * 1) * (64 * 1) * (1 * 2) = 0x02000 bytes
	   (32 * 1) * (32 * 1) * (1 * 2) = 0x00800 bytes
	   (64 * 1) * (64 * 1) * (2 * 2) = 0x04000 bytes
	   (32 * 1) * (32 * 1) * (2 * 2) = 0x01000 bytes
	   changed
	   (64 * 2) * (64 * 1) * (1 * 2) = 0x04000 bytes
	   (32 * 2) * (32 * 1) * (1 * 2) = 0x01000 bytes
	   (64 * 2) * (64 * 1) * (2 * 2) = 0x08000 bytes
	   (32 * 2) * (32 * 1) * (2 * 2) = 0x02000 bytes
	   changed
	   (64 * 2) * (64 * 1) * (1 * 2) = 0x08000 bytes
	   (32 * 2) * (32 * 1) * (1 * 2) = 0x02000 bytes
	   (64 * 2) * (64 * 1) * (2 * 2) = 0x10000 bytes
	   (32 * 2) * (32 * 1) * (2 * 2) = 0x04000 bytes
	*/

	plsize_bytes = (pltiles_x * pltiles_y) * ((2-current_tilemap.pattern_data_size)*2);

	/*---------------------------------------------------------------------------
	| Plane Size | Pattern Name Data Size | Character Size | Map Bits / Address |
	-----------------------------------------------------------------------------
	| 1 H x 1 V   see above, nothing has changed                                |
	-----------------------------------------------------------------------------
	|            |                        | 1 H x 1 V      | bits 6-1 * 0x04000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-1 * 0x01000 |
	| 2 H x 1 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-1 * 0x08000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-1 * 0x02000 |
	-----------------------------------------------------------------------------
	|            |                        | 1 H x 1 V      | bits 6-2 * 0x08000 |
	|            | 1 word                 |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-2 * 0x02000 |
	| 2 H x 2 V  ---------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-2 * 0x10000 |
	|            | 2 words                |-------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-2 * 0x04000 |
	--the-highest-bit-is-ignored-if-vram-is-only-4mbits------------------------*/


	/* Work out the Map Sizes in tiles, Map Dimensions */
	/* maps are always enabled? */
	if ( current_tilemap.map_count == 4 )
	{
		mptiles_x = pltiles_x * 2;
		mptiles_y = pltiles_y * 2;
		mppixels_x = plpixels_x * 2;
		mppixels_y = plpixels_y * 2;
	}
	else
	{
		mptiles_x = pltiles_x * 4;
		mptiles_y = pltiles_y * 4;
		mppixels_x = plpixels_x * 4;
		mppixels_y = plpixels_y * 4;
	}

	/* Map Size in BYTES */
	mpsize_bytes = (mptiles_x * mptiles_y) * ((2-current_tilemap.pattern_data_size)*2);


	/*-----------------------------------------------------------------------------------------------------------
	|            |                        | 1 H x 1 V      | bits 6-1 (upper mask 0x07f) (0x1ff >> 2) * 0x04000 |
	|            | 1 word                 |---------------------------------------------------------------------|
	|            |                        | 2 H x 2 V      | bits 8-1 (upper mask 0x1ff) (0x1ff >> 0) * 0x01000 |
	| 2 H x 1 V  -----------------------------------------------------------------------------------------------|
	|            |                        | 1 H x 1 V      | bits 5-1 (upper mask 0x03f) (0x1ff >> 3) * 0x08000 |
	|            | 2 words                |---------------------------------------------------------------------|
	|            |                        | 2 H x 2 V      | bits 7-1 (upper mask 0x0ff) (0x1ff >> 1) * 0x02000 |
	-------------------------------------------------------------------------------------------------------------
	lower mask = ~current_tilemap.plane_size
	-----------------------------------------------------------------------------------------------------------*/

	/* Precalculate bases from MAP registers */
	for (i = 0; i < current_tilemap.map_count; i++)
	{
		static const int shifttable[4] = {0,1,2,2};

		int uppermask, uppermaskshift;

		uppermaskshift = (1-current_tilemap.pattern_data_size) | ((1-current_tilemap.tile_size)<<1);
		uppermask = 0x1ff >> uppermaskshift;

		base[i] = ((current_tilemap.map_offset[i] & uppermask) >> shifttable[current_tilemap.plane_size]) * plsize_bytes;

		base[i] &= 0x7ffff; /* shienryu needs this for the text layer, is there a problem elsewhere or is it just right without the ram cart */

		base[i] = base[i] / 4; /* convert bytes to DWORDS */
	}

	/* other bits */
	/*current_tilemap.trans_enabled = current_tilemap.trans_enabled ? STV_TRANSPARENCY_NONE : STV_TRANSPARENCY_PEN; */
	current_tilemap.scrollx &= mppixels_x-1;
	current_tilemap.scrolly &= mppixels_y-1;

	pgsize_dwords = pgsize_bytes /4;
	/*plsize_dwords = plsize_bytes /4; */
	mpsize_dwords = mpsize_bytes /4;

	if (!current_tilemap.enabled) return; /* stop right now if its disabled ... */

	/* most things we need (or don't need) to work out are now worked out */

	for (y = 0; y<mptiles_y; y++) {
		int ypageoffs;
		int page, map, newbase, offs, data;
		int tilecode, flipyx, pal, gfx = 0;

		map = 0 ; page = 0 ;
		if ( y == 0 )
		{
			int drawyposinc = tilesizey*(current_tilemap.tile_size ? 2 : 1);
			drawypos = -(current_tilemap.scrolly*scaley);
			while( ((drawypos + drawyposinc) >> 16) < cliprect->min_y )
			{
				drawypos += drawyposinc;
				y++;
			}
			mptiles_y += y;
		}
		else
		{
			drawypos += tilesizey*(current_tilemap.tile_size ? 2 : 1);
		}
		if ((drawypos >> 16) > cliprect->max_y) break;

		ypageoffs = y & (pgtiles_y-1);

		for (x = 0; x<mptiles_x; x++) {
			int xpageoffs;
			int tilecodespacing = 1;

			if ( x == 0 )
			{
				int drawxposinc = tilesizex*(current_tilemap.tile_size ? 2 : 1);
				drawxpos = -(current_tilemap.scrollx*scalex);
				while( ((drawxpos + drawxposinc) >> 16) < cliprect->min_x )
				{
					drawxpos += drawxposinc;
					x++;
				}
				mptiles_x += x;
			}
			else
			{
				drawxpos+=tilesizex*(current_tilemap.tile_size ? 2 : 1);
			}
			if ( (drawxpos >> 16) > cliprect->max_x ) break;

			xpageoffs = x & (pgtiles_x-1);

			vdp2_get_map_page(x,y,&map,&page);

			newbase = base[map] + page * pgsize_dwords;
			offs = (ypageoffs * pgtiles_x) + xpageoffs;

			if (vdp2_cell_redraw.active)
			{
				/* only the cells with a rewritten pattern name, cleared first like a full rebuild */
				const UINT32 addr = newbase + ((current_tilemap.pattern_data_size == 1) ? (offs / 2) : offs);
				const int cellsize = current_tilemap.tile_size ? 16 : 8;
				struct rectangle cell;

				if (addr < vdp2_cell_redraw.min || addr >= vdp2_cell_redraw.max)
					continue;

				cell.min_x = drawxpos >> 16;
				cell.min_y = drawypos >> 16;
				cell.max_x = cell.min_x + cellsize - 1;
				cell.max_y = cell.min_y + cellsize - 1;
				if (cell.min_x < cliprect->min_x) cell.min_x = cliprect->min_x;
				if (cell.min_y < cliprect->min_y) cell.min_y = cliprect->min_y;
				if (cell.max_x > cliprect->max_x) cell.max_x = cliprect->max_x;
				if (cell.max_y > cliprect->max_y) cell.max_y = cliprect->max_y;
				if (cell.min_x <= cell.max_x && cell.min_y <= cell.max_y)
					bitmap_fill32(bitmap, &cell, 0);
			}

/* GET THE TILE INFO ... */
			/* 1 word per tile mode with supplement bits */
			if (current_tilemap.pattern_data_size ==1)
			{
				data = vdp2_vram[newbase + offs/2];
				data = (offs&1) ? (data & 0x0000ffff) : ((data & 0xffff0000) >> 16);

				/* Supplement Mode 12 bits, no flip */
				if (current_tilemap.character_number_supplement == 1)
				{
/* no flip */       flipyx   = 0;
/* 8x8 */           if (current_tilemap.tile_size==0) tilecode = (data & 0x0fff) + ( (current_tilemap.supplementary_character_bits&0x1c) << 10);
/* 16x16 */         else tilecode = ((data & 0x0fff) << 2) + (current_tilemap.supplementary_character_bits&0x03) + ((current_tilemap.supplementary_character_bits&0x10) << 10);
				}
				/* Supplement Mode 10 bits, with flip */
				else
				{
/* flip bits */     flipyx   = (data & 0x0c00) >> 10;
/* 8x8 */           if (current_tilemap.tile_size==0) tilecode = (data & 0x03ff) +  ( (current_tilemap.supplementary_character_bits) << 10);
/* 16x16 */         else tilecode = ((data & 0x03ff) <<2) +  (current_tilemap.supplementary_character_bits&0x03) + ((current_tilemap.supplementary_character_bits&0x1c) << 10);
				}

/*>16cols*/     if (current_tilemap.colour_depth != 0) pal = ((data & 0x7000)>>8);
/*16 cols*/     else pal = ((data & 0xf000)>>12) +( (current_tilemap.supplementary_palette_bits) << 4);

			}
			/* 2 words per tile, no supplement bits */
			else
			{
				data = vdp2_vram[newbase + offs];
				tilecode = (data & 0x00007fff);
				pal   = (data &    0x007f0000)>>16;
	/*          specialc = (data & 0x10000000)>>28; */
				flipyx   = (data & 0xc0000000)>>30;
			}
/* WE'VE GOT THE TILE INFO ... */

			if ( tilecode < tilecodemin ) tilecodemin = tilecode;
			if ( tilecode > tilecodemax ) tilecodemax = tilecode;

/* DECODE ANY TILES WE NEED TO DECODE */

			pal += current_tilemap.colour_ram_address_offset<< 4; /* bios uses this .. */

			if ( pal < palmin )
			{
				palmin = pal;
			}
			if ( pal > palmax )
			{
				palmax = pal;
			}

			/*Enable fading bit*/
			if(current_tilemap.fade_control & 1)
			{
				/*Select fading bit*/
				pal += ((current_tilemap.fade_control & 2) ? (0x100) : (0x80));
			}

			if (current_tilemap.colour_depth == 1)
			{
				gfx = 2;
				pal = pal >>4;
				tilecode &=0x7fff;
				if (tilecode == 0x7fff) tilecode--; /* prevents crash but unsure what should happen; wrapping? */
				tilecodespacing = 2;
			}
			else if (current_tilemap.colour_depth == 0)
			{
				gfx = 0;
				tilecode &=0x7fff;
				tilecodespacing = 1;
			}
/* TILES ARE NOW DECODED */

			if(!get_vramsz())
				tilecode &= 0x3fff;

/* DRAW! */
			if(current_tilemap.incx != 0x10000 ||
				current_tilemap.incy != 0x10000 ||
				current_tilemap.transparency & STV_TRANSPARENCY_ADD_BLEND )
			{
#define SCR_TILESIZE_X          (((drawxpos + tilesizex) >> 16) - (drawxpos >> 16))
#define SCR_TILESIZE_X1(startx) (((drawxpos + (startx) + tilesizex) >> 16) - ((drawxpos + (startx))>>16))
#define SCR_TILESIZE_Y          (((drawypos + tilesizey) >> 16) - (drawypos >> 16))
#define SCR_TILESIZE_Y1(starty) (((drawypos + (starty) + tilesizey) >> 16) - ((drawypos + (starty))>>16))
				if (current_tilemap.tile_size==1)
				{
					if ( current_tilemap.colour_depth == 4 )
					{
						popmessage("Unsupported tilemap gfx zoom color depth = 4, tile size = 1");
					}
					else if ( current_tilemap.colour_depth == 3 )
					{
						/* RGB555 */
						vdp2_drawgfxzoom_rgb555(bitmap,cliprect,tilecode+(0+(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos >> 16, drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X, SCR_TILESIZE_Y,current_tilemap.alpha);
						vdp2_drawgfxzoom_rgb555(bitmap,cliprect,tilecode+(1-(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,(drawxpos+tilesizex) >> 16,drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X1(tilesizex), SCR_TILESIZE_Y,current_tilemap.alpha);
						vdp2_drawgfxzoom_rgb555(bitmap,cliprect,tilecode+(2+(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos >> 16,(drawypos+tilesizey) >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X, SCR_TILESIZE_Y1(tilesizey),current_tilemap.alpha);
						vdp2_drawgfxzoom_rgb555(bitmap,cliprect,tilecode+(3-(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,(drawxpos+tilesizex)>> 16,(drawypos+tilesizey) >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X1(tilesizex), SCR_TILESIZE_Y1(tilesizey),current_tilemap.alpha);
					}
					else
					{
						/* normal */
						vdp2_drawgfxzoom(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(0+(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos >> 16, drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X, SCR_TILESIZE_Y,current_tilemap.alpha);
						vdp2_drawgfxzoom(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(1-(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,(drawxpos+tilesizex) >> 16,drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X1(tilesizex), SCR_TILESIZE_Y,current_tilemap.alpha);
						vdp2_drawgfxzoom(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(2+(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos >> 16,(drawypos+tilesizey) >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X, SCR_TILESIZE_Y1(tilesizey),current_tilemap.alpha);
						vdp2_drawgfxzoom(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(3-(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,(drawxpos+tilesizex)>> 16,(drawypos+tilesizey) >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X1(tilesizex), SCR_TILESIZE_Y1(tilesizey),current_tilemap.alpha);
					}
				}
				else
				{
					if ( current_tilemap.colour_depth == 4 )
						popmessage("Unsupported tilemap gfx zoom color depth = 4, tile size = 0");
					else if ( current_tilemap.colour_depth == 3)
					{
						vdp2_drawgfxzoom_rgb555(bitmap,cliprect,tilecode,pal,flipyx&1,flipyx&2, drawxpos >> 16, drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X,SCR_TILESIZE_Y,current_tilemap.alpha);
					}
					else
						vdp2_drawgfxzoom(bitmap,cliprect,(&stv_gfx[gfx]),tilecode,pal,flipyx&1,flipyx&2, drawxpos >> 16, drawypos >> 16,current_tilemap.transparency,scalex,scaley,SCR_TILESIZE_X,SCR_TILESIZE_Y,current_tilemap.alpha);
				}
			}
			else
			{
				int olddrawxpos, olddrawypos;
				olddrawxpos = drawxpos; drawxpos >>= 16;
				olddrawypos = drawypos; drawypos >>= 16;
				if (current_tilemap.tile_size==1)
				{
					if ( current_tilemap.colour_depth == 4 )
					{
						/* normal, an 8x8 RGB888 cell takes 8 character number units (32 bytes each) */
						vdp2_drawgfx_rgb888(bitmap,cliprect,tilecode+(0+(flipyx&1)+(flipyx&2))*8,flipyx&1,flipyx&2,drawxpos, drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb888(bitmap,cliprect,tilecode+(1-(flipyx&1)+(flipyx&2))*8,flipyx&1,flipyx&2,drawxpos+8,drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb888(bitmap,cliprect,tilecode+(2+(flipyx&1)-(flipyx&2))*8,flipyx&1,flipyx&2,drawxpos,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb888(bitmap,cliprect,tilecode+(3-(flipyx&1)-(flipyx&2))*8,flipyx&1,flipyx&2,drawxpos+8,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
					}
					else if ( current_tilemap.colour_depth == 3 )
					{
						/* normal */
						vdp2_drawgfx_rgb555(bitmap,cliprect,tilecode+(0+(flipyx&1)+(flipyx&2))*4,flipyx&1,flipyx&2,drawxpos, drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb555(bitmap,cliprect,tilecode+(1-(flipyx&1)+(flipyx&2))*4,flipyx&1,flipyx&2,drawxpos+8,drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb555(bitmap,cliprect,tilecode+(2+(flipyx&1)-(flipyx&2))*4,flipyx&1,flipyx&2,drawxpos,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_rgb555(bitmap,cliprect,tilecode+(3-(flipyx&1)-(flipyx&2))*4,flipyx&1,flipyx&2,drawxpos+8,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
					}
					else if (current_tilemap.transparency & STV_TRANSPARENCY_ALPHA)
					{
						/* alpha */
						vdp2_drawgfx_alpha(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(0+(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos, drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_alpha(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(1-(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos+8,drawypos,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_alpha(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(2+(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
						vdp2_drawgfx_alpha(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(3-(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos+8,drawypos+8,current_tilemap.transparency,current_tilemap.alpha);
					}
					else
					{
						/* normal */
						vdp2_drawgfx_transpen(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(0+(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos, drawypos,current_tilemap.transparency);
						vdp2_drawgfx_transpen(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(1-(flipyx&1)+(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos+8,drawypos,current_tilemap.transparency);
						vdp2_drawgfx_transpen(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(2+(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos,drawypos+8,current_tilemap.transparency);
						vdp2_drawgfx_transpen(bitmap,cliprect,(&stv_gfx[gfx]),tilecode+(3-(flipyx&1)-(flipyx&2))*tilecodespacing,pal,flipyx&1,flipyx&2,drawxpos+8,drawypos+8,current_tilemap.transparency);
					}
				}
				else
				{
					if ( current_tilemap.colour_depth == 4)
					{
						vdp2_drawgfx_rgb888(bitmap,cliprect,tilecode,flipyx&1,flipyx&2,drawxpos,drawypos,current_tilemap.transparency,current_tilemap.alpha);
					}
					else if ( current_tilemap.colour_depth == 3)
					{
						vdp2_drawgfx_rgb555(bitmap,cliprect,tilecode,flipyx&1,flipyx&2,drawxpos,drawypos,current_tilemap.transparency,current_tilemap.alpha);
					}
					else
					{
						if (current_tilemap.transparency & STV_TRANSPARENCY_ALPHA)
							vdp2_drawgfx_alpha(bitmap,cliprect,(&stv_gfx[gfx]),tilecode,pal,flipyx&1,flipyx&2, drawxpos, drawypos,current_tilemap.transparency,current_tilemap.alpha);
						else
							vdp2_drawgfx_transpen(bitmap,cliprect,(&stv_gfx[gfx]),tilecode,pal,flipyx&1,flipyx&2, drawxpos, drawypos,current_tilemap.transparency);
					}
				}
				drawxpos = olddrawxpos;
				drawypos = olddrawypos;
			}
/* DRAWN?! */

		}
	}
	if ( current_tilemap.layer_name & 0x80 )
	{
		static const int shifttable[4] = {0,1,2,2};
		int uppermask, uppermaskshift;
		int mapsize;
		uppermaskshift = (1-current_tilemap.pattern_data_size) | ((1-current_tilemap.tile_size)<<1);
		uppermask = 0x1ff >> uppermaskshift;

		log_masked(LOG_VDP2, "Layer RBG%d, size %d x %d\n", current_tilemap.layer_name & 0x7f, cliprect->max_x + 1, cliprect->max_y + 1);
		log_masked(LOG_VDP2, "Tiles: min %08X, max %08X\n", tilecodemin, tilecodemax);
		log_masked(LOG_VDP2, "MAP size in dwords %08X\n", mpsize_dwords);
		for (i = 0; i < current_tilemap.map_count; i++)
		{
			log_masked(LOG_VDP2, "Map register %d: base %08X\n", current_tilemap.map_offset[i], base[i]);
		}

		/* store map information */
		vdp2_layer_data.map_offset_min = 0x7fffffff;
		vdp2_layer_data.map_offset_max = 0x00000000;

		for (i = 0; i < current_tilemap.map_count; i++)
		{
			UINT32 max_base;

			if ( base[i] < vdp2_layer_data.map_offset_min )
				vdp2_layer_data.map_offset_min = base[i];

			/* Head On in Sega Memorial Collection 1 cares (uses RBG0 with all map regs equal to 0x20) */
			max_base = (base[i] + plsize_bytes/4);
			if (  max_base > vdp2_layer_data.map_offset_max )
				vdp2_layer_data.map_offset_max = max_base;
		}


		mapsize = ((1 & uppermask) >> shifttable[current_tilemap.plane_size]) * plsize_bytes -
					((0 & uppermask) >> shifttable[current_tilemap.plane_size]) * plsize_bytes;
		mapsize /= 4;

		vdp2_layer_data.map_offset_max += mapsize;

		vdp2_layer_data.tile_offset_min = tilecodemin * 0x20 / 4;
		vdp2_layer_data.tile_offset_max = (tilecodemax + 1) * 0x20 / 4;

		/* store the colours used as well, the RBG cache has them baked in */
		switch ( current_tilemap.colour_depth )
		{
			case 0:
				vdp2_layer_data.pen_min = palmin * 16;
				vdp2_layer_data.pen_max = (palmax + 1) * 16;
				break;
			case 1:
				vdp2_layer_data.pen_min = (palmin & ~0xf) * 16;
				vdp2_layer_data.pen_max = ((palmax & ~0xf) + 0x10) * 16;
				break;
			case 2:
				vdp2_layer_data.pen_min = 0;
				vdp2_layer_data.pen_max = 0x800;
				break;
			default:
				/* RGB, no palette */
				vdp2_layer_data.pen_min = 0;
				vdp2_layer_data.pen_max = 0;
				break;
		}
		if ( palmin > palmax )
		{
			vdp2_layer_data.pen_min = vdp2_layer_data.pen_max = 0;
		}
	}

}

#define VDP2_READ_VERTICAL_LINESCROLL( _val, _address ) \
	{ \
		_val = (INT32)((vdp2_vram[ _address ] & 0x07ffff00) << 5) >> 5; \
	}


static void vdp2_check_tilemap_with_linescroll(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	struct rectangle mycliprect;
	int cur_line = cliprect->min_y;
	int address;
	int active_functions = 0;
	INT32 scroll_values[3], prev_scroll_values[3];
	int i;
	int scroll_values_equal;
	int lines;
	INT16 main_scrollx, main_scrolly;
/*  INT32 incx; */
	int linescroll_enable, vertical_linescroll_enable, linezoom_enable;
	int vertical_linescroll_index = -1;

	/* read original scroll values */
	main_scrollx = current_tilemap.scrollx;
	main_scrolly = current_tilemap.scrolly;
/*  incx = current_tilemap.incx; */

	/* prepare linescroll flags */
	linescroll_enable = current_tilemap.linescroll_enable;
/*  current_tilemap.linescroll_enable = 0; */
	vertical_linescroll_enable = current_tilemap.vertical_linescroll_enable;
/*  current_tilemap.vertical_linescroll_enable = 0; */
	linezoom_enable = current_tilemap.linezoom_enable;
/*  current_tilemap.linezoom_enable = 0; */

	/* prepare working clipping rectangle */
	memcpy( &mycliprect, cliprect, sizeof(struct rectangle) );

	/* calculate the number of active functions */
	if ( linescroll_enable ) active_functions++;
	if ( vertical_linescroll_enable )
	{
		vertical_linescroll_index = active_functions;
		active_functions++;
	}
	if ( linezoom_enable ) active_functions++;

	/* address of data table: one entry per linescroll_interval lines (the threaded */
	/* strips start on a multiple of 16 lines, so on a multiple of the interval) */
	address = current_tilemap.linescroll_table_address + active_functions*4*(cliprect->min_y / (current_tilemap.linescroll_interval ? current_tilemap.linescroll_interval : 1));

	/* get the first scroll values */
	for ( i = 0; i < active_functions; i++ )
	{
		if ( i == vertical_linescroll_index )
		{
			VDP2_READ_VERTICAL_LINESCROLL( prev_scroll_values[i], (address / 4) + i );
			prev_scroll_values[i] -= (cur_line * current_tilemap.incy);
		}
		else
		{
			prev_scroll_values[i] = vdp2_vram[ (address / 4) + i ];
		}
	}

	while( cur_line <= cliprect->max_y )
	{
		lines = 0;
		do
		{
			/* update address */
			address += active_functions*4;

			/* update lines count */
			lines += current_tilemap.linescroll_interval;

			/* get scroll values */
			for ( i = 0; i < active_functions; i++ )
			{
				if ( i == vertical_linescroll_index )
				{
					VDP2_READ_VERTICAL_LINESCROLL( scroll_values[i], (address/4) + i );
					scroll_values[i] -= (cur_line + lines) * current_tilemap.incy;
				}
				else
				{
					scroll_values[i] = vdp2_vram[ (address / 4) + i ];
				}
			}

			/* compare scroll values */
			scroll_values_equal = 1;
			for ( i = 0; i < active_functions; i++ )
			{
				scroll_values_equal &= (scroll_values[i] == prev_scroll_values[i]);
			}
		} while( scroll_values_equal && ((cur_line + lines) <= cliprect->max_y) );

		/* determined how many lines can be drawn */
		/* prepare clipping rectangle */
		mycliprect.min_y = cur_line; mycliprect.max_y = cur_line + lines - 1;

		/* prepare scroll values */
		i = 0;
		/* linescroll */
		if ( linescroll_enable )
		{
			prev_scroll_values[i] = (INT32)((prev_scroll_values[i] & 0x07ffff00) << 5) >> 5;
			current_tilemap.scrollx = main_scrollx + (prev_scroll_values[i] >> 16);
			i++;
		}
		/* vertical line scroll */
		if ( vertical_linescroll_enable )
		{
			current_tilemap.scrolly = main_scrolly + (prev_scroll_values[i] >> 16);
			i++;
		}

		/* linezooom */
		if ( linezoom_enable )
		{
			prev_scroll_values[i] = (INT32)((prev_scroll_values[i] & 0x0007ff00) << 13) >> 13;
			current_tilemap.incx = prev_scroll_values[i];
			i++;
		}

/*      log_masked(LOG_VDP2, "Linescroll: y < %d, %d >, scrollx = %d, scrolly = %d, incx = %f\n", mycliprect.min_y, mycliprect.max_y, current_tilemap.scrollx, current_tilemap.scrolly, (float)current_tilemap.incx/65536.0); */
		/* render current tilemap portion */
		if (current_tilemap.bitmap_enable) /* this layer is a bitmap */
		{
			vdp2_draw_basic_bitmap(bitmap, &mycliprect);
		}
		else
		{
			/*vdp2_apply_window_on_layer(mycliprect); */
			vdp2_draw_basic_tilemap(bitmap, &mycliprect);
		}

		/* update parameters for next iteration */
		memcpy( prev_scroll_values, scroll_values, sizeof(scroll_values));
		cur_line += lines;
	}
}

static void vdp2_draw_line(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int x,y;
	UINT8* gfxdata = vdp2l.gfx_decode;
	UINT32 base_offs,base_mask;
	UINT32 pix;
	UINT8 interlace;

	interlace = (get_lsmd() == 3) + 1;

	{
		base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;

		for(y=cliprect->min_y;y<=cliprect->max_y;y++)
		{
			base_offs = (VDP2_LCTA & base_mask) << 1;

			if(VDP2_LCCLMD)
				base_offs += (y / interlace) << 1;

			for(x=cliprect->min_x;x<=cliprect->max_x;x++)
			{
				UINT16 pen;

				pen = (gfxdata[base_offs+0]<<8)|gfxdata[base_offs+1];
				pix = BMP32(bitmap, y)[x];

				BMP32(bitmap, y)[x] = add_blend_r32(PEN(pen & 0x7ff),pix);
			}
		}
	}
}

static void vdp2_draw_mosaic(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 is_roz)
{
	int y;
	int x;
	int yi;
	int xi;
	UINT8 h_size = VDP2_MZSZH+1;
	UINT8 v_size = VDP2_MZSZV+1;

	if(is_roz)
		v_size = 1;

	if(h_size == 1 && v_size == 1)
		return; /* don't bother */

	if(get_lsmd() == 3)
		v_size <<= 1;

	for(y=cliprect->min_y;y<=cliprect->max_y;y+=v_size)
	{
		for(x=cliprect->min_x;x<=cliprect->max_x;x+=h_size)
		{
			UINT32 pix = BMP32(bitmap, y)[x];

			for(yi=0;yi<v_size;yi++)
				for(xi=0;xi<h_size;xi++)
					BMP32(bitmap, y+yi)[x+xi] = pix;
		}
	}
}

static void vdp2_check_tilemap(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/* the idea is here we check the tilemap capabilities / whats enabled and call an appropriate tilemap drawing routine, or
	  at the very list throw up a few errors if the tilemaps want to do something we don't support yet */
/*  int window_applied = 0; */
	struct rectangle mycliprect = *cliprect;

	/* the prepass only brings the rotation plane caches up to date */
	if (vdp2_render_mode == VDP2_RENDER_PREPASS && bitmap != vdp2l.roz_bitmap[0] && bitmap != vdp2l.roz_bitmap[1])
		return;

/*	if (current_tilemap.vertical_cell_scroll_enable) */
/*		popmessage("%d %d %d %d", current_tilemap.linescroll_enable, current_tilemap.vertical_linescroll_enable, current_tilemap.linezoom_enable, current_tilemap.vertical_cell_scroll_enable); */

	/* check for vertical cell scroll enable (sonicjamj) */
	/* TODO: it is unknown how this works with vertical linescroll enable too (it may not work?) */
	/* TODO: support a subset only for now, given batmanfr The Riddler stage also sets linezoom_enable */
	/* Would make the background rounded to the Dome, but o(n*m) nested loop causes a performance nosedive */
	/* https://mametesters.org/view.php?id=7203 */
	if (current_tilemap.linescroll_enable && current_tilemap.vertical_cell_scroll_enable && !current_tilemap.vertical_linescroll_enable && !current_tilemap.linezoom_enable)
	{
		UINT32 vcsc_address;
		UINT32 base_mask;
		int base_offset, base_multiplier;
		INT16 base_scrollx, base_scrolly;
		/*UINT32 base_incx, base_incy; */
		int cur_char = 0;

		base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;
		vcsc_address = (((VDP2_VCSTAU << 16) | VDP2_VCSTAL) & base_mask) * 2;
		vcsc_address >>= 2;

		base_offset = 0;
		base_multiplier = 1;
		/* offset for both enabled */
		if(VDP2_N0VCSC && VDP2_N1VCSC)
		{
			/* NBG1 */
			if(current_tilemap.layer_name & 1)
				base_offset = 1;

			base_multiplier = 2;
		}

		base_scrollx = current_tilemap.scrollx;
		base_scrolly = current_tilemap.scrolly;
		/*base_incx = current_tilemap.incx; */
		/*base_incy = current_tilemap.incy; */

		while(cur_char <= cliprect->max_x)
		{
			UINT32 cur_address;
			INT16 char_scroll;

			mycliprect.min_x = cur_char;
			mycliprect.max_x = cur_char + 8 - 1;

			cur_address = vcsc_address;
			cur_address += ((cur_char >> 3) * base_multiplier) + base_offset;

			char_scroll = vdp2_vram[ cur_address ] >> 16;
			char_scroll &= 0x07ff;
			if ( char_scroll & 0x0400 ) char_scroll |= 0xf800;
			current_tilemap.scrollx = base_scrollx;
			current_tilemap.scrolly = base_scrolly + (char_scroll);
			/*current_tilemap.incx = base_incx; */
			/*current_tilemap.incy = base_incy; */

			vdp2_check_tilemap_with_linescroll(bitmap, &mycliprect);

			/* TODO: + 16 for tilemap and char size = 16? */
			cur_char += 8;

		}

		return;
	}
	else if ( current_tilemap.linescroll_enable ||
			current_tilemap.vertical_linescroll_enable ||
			current_tilemap.linezoom_enable)
	{
		vdp2_check_tilemap_with_linescroll(bitmap, cliprect);

		return;
	}

	if (current_tilemap.bitmap_enable) /* this layer is a bitmap */
	{
		vdp2_draw_basic_bitmap(bitmap, &mycliprect);
	}
	else
	{
		/*vdp2_apply_window_on_layer(mycliprect); */
		vdp2_draw_basic_tilemap(bitmap, &mycliprect);
	}

	/* post-processing functions */
	/* (TODO: needs layer bitmaps to be individual planes to work correctly) */
	if(current_tilemap.line_screen_enabled && TEST_FUNCTIONS)
		vdp2_draw_line(bitmap,cliprect);

	if(current_tilemap.mosaic_screen_enabled && TEST_FUNCTIONS)
		vdp2_draw_mosaic(bitmap,cliprect,current_tilemap.layer_name & 0x80);


	{
		if(current_tilemap.colour_depth == 2 && !current_tilemap.bitmap_enable)
			popmessage("2048 color mode used on a non-bitmap plane");

/*      if(VDP2_SCXDN0 || VDP2_SCXDN1 || VDP2_SCYDN0 || VDP2_SCYDN1) */
/*          popmessage("Fractional part scrolling write"); */

		/* capgen2 - Choh Makaimura (obviously) */
		if(VDP2_MZCTL & 0x1f && POPMESSAGE_DEBUG)
			popmessage("Mosaic control enabled = %04x\n",VDP2_MZCTL);

		/* revil/biohaz bit 1 */
		/* airsadve 0x3e */
		/* bakhunt */
		if(VDP2_LNCLEN & ~2 && POPMESSAGE_DEBUG)
			popmessage("Line Colour screen enabled %04x %08x",VDP2_LNCLEN,VDP2_LCTAU<<16|VDP2_LCTAL);

		/* revil/biohaz 0x400 = extended color calculation enabled */
		/* aww 0x200 = color calculation ratio mode */
		/* whizz/whizzj = 0x8100 */
		/* darksavu = 0x9051 on save select screen (the one with a Saturn in the background) */
		if(VDP2_CCCR & 0x6000)
			popmessage("Gradation enabled %04x",VDP2_CCCR);

		/* Advanced VG, Shining Force III */
		if(VDP2_SFCCMD && POPMESSAGE_DEBUG)
			popmessage("Special Color Calculation enable %04x",VDP2_SFCCMD);

		/* cleopatr Transparent Shadow */
		/* prettyx Back & Transparent Shadow*/
		/*if(VDP2_SDCTL & 0x0120) */
		/*  popmessage("%s shadow select bit enabled",VDP2_SDCTL & 0x100 ? "Transparent" : "Back"); */

		/* lengris3 bit 3 normal, bit 1 during battle field */
		/* mslug bit 0 during gameplay */
		/* bugu Sega Away Logo onward 0x470 */
		/* cncu 0x0004 0xc000, azelpanztai 0x0004 0x0000 (FMV) */
		if(VDP2_SFSEL & ~0x47f)
			popmessage("Special Function Code Select enable %04x %04x",VDP2_SFSEL,VDP2_SFCODE);

		/* albodys 0x0001 */
		/* asuka120 0x0101 */
		/* slamnjamu 0x0003 */
		if(VDP2_ZMCTL & 0x0200)
			popmessage("Reduction enable %04x",VDP2_ZMCTL);

		/* burningru based FMVs, jltsuk backgrounds */
		if(VDP2_SCRCTL & 0x0101 && POPMESSAGE_DEBUG)
			popmessage("Vertical cell scroll enable %04x",VDP2_SCRCTL);

		/* magdrop3 0x200 -> color calculation window */
		/* ideyusmj 0x0303 */
		/* decathlt 0x088 */
		/* sexyparo 0x2300 */
/*      if(VDP2_WCTLD & 0x2000) */
/*          popmessage("Special window enabled %04x",VDP2_WCTLD); */

		/* shinfrc3u, aburner2 (doesn't make a proper use tho?) */
		/* layersec */
		/*if(VDP2_W0LWE || VDP2_W1LWE) */
		/*  popmessage("Line Window %s %08x enabled",VDP2_W0LWE ? "0" : "1",VDP2_W0LWTA); */

		/* draculax bits 2-4 */
		/* acstrike bit 5 */
		/* capgen2 - Choh Makaimura 0x0055 */
		/* srallycu 0x0155 */
		/* findlove 0x4400 */
		/* dbzsbuto 0x3800 - 0x2c00 */
		/* leynos2 0x0200*/
		/* bugu 0x8800 */
		/* wonder3 0x0018 */
		if(VDP2_SFPRMD & ~0xff7f)
			popmessage("Special Priority Mode enabled %04x",VDP2_SFPRMD);
	}
}


#define ROZ_PREFETCH	8

/*
	The rotation windows of line y for x in [min_x, max_x]: 1 where the
	pixel goes through (the rotation windows of R0W0E/R0W1E, and the
	rotation parameter windows of RPW0E/RPW1E in mode 3), with the window coordinates and
	settings read once for the line. Returns 0 when no window is enabled
	(all the pixels go through), without filling the mask.
*/
static int vdp2_roz_window_line(UINT8 *mask, int min_x, int max_x, int y, int mode3, int rot_parameter)
{
	int s0 = 0, e0 = 0, sy0 = 0, ey0 = 0, s1 = 0, e1 = 0, sy1 = 0, ey1 = 0, x;
	UINT8 logic = VDP2_R0LOG, w0e = VDP2_R0W0E, w1e = VDP2_R0W1E, w0a = VDP2_R0W0A, w1a = VDP2_R0W1A;
	UINT8 mlogic = VDP2_RPLOG, mw0e = VDP2_RPW0E, mw1e = VDP2_RPW1E, mw0a = VDP2_RPW0A, mw1a = VDP2_RPW1A;
	int roz = (w0e || w1e), m3 = mode3 && (mw0e || mw1e);

	if (!roz && !mode3)
		return 0;
	if (roz || m3)
	{
		vdp2_get_window0_coordinates(&s0, &e0, &sy0, &ey0, y);
		vdp2_get_window1_coordinates(&s1, &e1, &sy1, &ey1, y);
	}
	for (x = min_x; x <= max_x; x++)
	{
		int pass = 1;
		if (roz)
		{
			int w0 = get_roz_window_pixel(s0, e0, sy0, ey0, x, y, w0e, w0a);
			int w1 = get_roz_window_pixel(s1, e1, sy1, ey1, x, y, w1e, w1a);
			pass = logic & 1 ? (w0 | w1) : (w0 & w1);
		}
		if (pass && mode3)
		{
			if (m3)
			{
				int w0 = get_roz_window_pixel(s0, e0, sy0, ey0, x, y, mw0e, mw0a);
				int w1 = get_roz_window_pixel(s1, e1, sy1, ey1, x, y, mw1e, mw1a);
				pass = (mlogic & 1 ? (w0 | w1) : (w0 & w1)) ^ rot_parameter;
			}
			else
				pass = rot_parameter ^ 1;
		}
		mask[x] = pass;
	}
	return 1;
}

static void vdp2_copy_roz_bitmap(struct mame_bitmap *bitmap,
										struct mame_bitmap *roz_bitmap,
										const struct rectangle *cliprect,
										int iRP,
										int planesizex,
										int planesizey,
										int planerenderedsizex,
										int planerenderedsizey)
{
	INT32 xsp, ysp, xp, yp, dx, dy, x, y, xs, ys, dxs, dys;
	INT32 vcnt, hcnt;
	INT32 kx, ky;
	INT8  use_coeff_table, coeff_table_mode, coeff_table_size, coeff_table_shift;
	INT8  screen_over_process;
	UINT8 vcnt_shift, hcnt_shift;
	UINT8 coeff_msb;
	UINT32 *coeff_table_base, coeff_table_offset;
	INT32 coeff_table_val;
	UINT32 address;
	UINT32 *line;
	rgb_t pix;
	/*UINT32 coeff_line_color_screen_data; */
	INT32 clipxmask = 0, clipymask = 0;
	int co_r = 0, co_g = 0, co_b = 0;
	UINT8 winmask[1024];
	int use_winmask;
	int transparency, fade;
	UINT8 alpha;
	/* the plane's pixels without the line table (one dependent load less per pixel) */
	const UINT32 *rozbase = (const UINT32 *)roz_bitmap->base;
	const int rozpitch = roz_bitmap->rowpixels;


	if (current_tilemap.fade_control & 1)
		vdp2_color_offset_values(current_tilemap.fade_control & 2, &co_r, &co_g, &co_b);
	vcnt_shift = get_lsmd() == 3;
	hcnt_shift = BIT(get_hreso(), 1);

	planesizex--;
	planesizey--;
	planerenderedsizex--;
	planerenderedsizey--;

	kx = RP.kx;
	ky = RP.ky;

	use_coeff_table = coeff_table_mode = coeff_table_size = coeff_table_shift = 0;
	coeff_table_offset = 0;
	coeff_table_val = 0;
	coeff_table_base = NULL;

	log_masked(LOG_ROZ, "Rendering RBG with parameter %s\n", iRP == 1 ? "A" : "B");
	log_masked(LOG_ROZ, "RPMD (parameter mode) = %x\n", VDP2_RPMD);
	log_masked(LOG_ROZ, "RPRCTL (parameter read control) = %04x\n", VDP2_RPRCTL);
	log_masked(LOG_ROZ, "KTCTL (coefficient table control) = %04x\n", VDP2_KTCTL);
	log_masked(LOG_ROZ, "KTAOF (coefficient table address offset) = %04x\n", VDP2_KTAOF);
	log_masked(LOG_ROZ, "RAOVR (screen-over process) = %x\n", VDP2_RAOVR);
	if ( iRP == 1 )
	{
		use_coeff_table = VDP2_RAKTE;
		if ( use_coeff_table == 1 )
		{
			coeff_table_mode = VDP2_RAKMD;
			coeff_table_size = VDP2_RAKDBS;
			coeff_table_offset = VDP2_RAKTAOS;
		}
		screen_over_process = VDP2_RAOVR;
	}
	else
	{
		use_coeff_table = VDP2_RBKTE;
		if ( use_coeff_table == 1 )
		{
			coeff_table_mode = VDP2_RBKMD;
			coeff_table_size = VDP2_RBKDBS;
			coeff_table_offset = VDP2_RBKTAOS;
		}
		screen_over_process = VDP2_RBOVR;
	}
	if ( use_coeff_table )
	{
		if ( VDP2_CRKTE == 0 )
		{
			coeff_table_base = vdp2_vram;
		}
		else
		{
			coeff_table_base = vdp2_cram;
		}
		if ( coeff_table_size == 0 )
		{
			coeff_table_offset = (coeff_table_offset & 0x0003) * 0x40000;
			coeff_table_shift = 2;
		}
		else
		{
			coeff_table_offset = (coeff_table_offset & 0x0007) * 0x20000;
			coeff_table_shift = 1;
		}
	}

	if ( current_tilemap.colour_calculation_enabled == 1 )
	{
		if ( VDP2_CCMD )
		{
			current_tilemap.transparency |= STV_TRANSPARENCY_ADD_BLEND;
		}
		else
		{
			current_tilemap.transparency |= STV_TRANSPARENCY_ALPHA;
		}
	}

	/* clipping */
	switch( screen_over_process )
	{
		case 0:
			/* repeated */
			clipxmask = clipymask = 0;
			break;
		case 1:
			/* screen over pattern */
			/* TODO: not supported, cfr. VDP2_OVPNRA / VDP2_OVPNRB */
			/* D-Xhird uses this on practice stage */
			clipxmask = ~planesizex;
			clipymask = ~planesizey;
			break;
		case 2:
			/* outside display area, scroll screen is transparent */
			clipxmask = ~planesizex;
			clipymask = ~planesizey;
			break;
		case 3:
			/* display area is 512x512, outside is transparent */
			clipxmask = ~511;
			clipymask = ~511;
			break;
	}

	/*dx  = (RP.A * RP.dx) + (RP.B * RP.dy); */
	/*dy  = (RP.D * RP.dx) + (RP.E * RP.dy); */
	dx = mul_fixed32( RP.A, RP.dx ) + mul_fixed32( RP.B, RP.dy );
	dy = mul_fixed32( RP.D, RP.dx ) + mul_fixed32( RP.E, RP.dy );

	/*xp  = RP.A * ( RP.px - RP.cx ) + RP.B * ( RP.py - RP.cy ) + RP.C * ( RP.pz - RP.cz ) + RP.cx + RP.mx; */
	/*yp  = RP.D * ( RP.px - RP.cx ) + RP.E * ( RP.py - RP.cy ) + RP.F * ( RP.pz - RP.cz ) + RP.cy + RP.my; */
	xp = mul_fixed32( RP.A, RP.px - RP.cx ) + mul_fixed32( RP.B, RP.py - RP.cy ) + mul_fixed32( RP.C, RP.pz - RP.cz ) + RP.cx + RP.mx;
	yp = mul_fixed32( RP.D, RP.px - RP.cx ) + mul_fixed32( RP.E, RP.py - RP.cy ) + mul_fixed32( RP.F, RP.pz - RP.cz ) + RP.cy + RP.my;

	for (vcnt = cliprect->min_y; vcnt <= cliprect->max_y; vcnt++ )
	{
		/*xsp = RP.A * ( ( RP.xst + RP.dxst * (vcnt << 16) ) - RP.px ) +
		      RP.B * ( ( RP.yst + RP.dyst * (vcnt << 16) ) - RP.py ) +
		      RP.C * ( RP.zst - RP.pz);
		ysp = RP.D * ( ( RP.xst + RP.dxst * (vcnt << 16) ) - RP.px ) +
		      RP.E * ( ( RP.yst + RP.dyst * (vcnt << 16) ) - RP.py ) +
		      RP.F * ( RP.zst - RP.pz );*/
		xsp = mul_fixed32( RP.A, RP.xst + mul_fixed32( RP.dxst, vcnt << (16 - vcnt_shift)) - RP.px ) +
				mul_fixed32( RP.B, RP.yst + mul_fixed32( RP.dyst, vcnt << (16 - vcnt_shift)) - RP.py ) +
				mul_fixed32( RP.C, RP.zst - RP.pz );
		ysp = mul_fixed32( RP.D, RP.xst + mul_fixed32( RP.dxst, vcnt << (16 - vcnt_shift)) - RP.px ) +
				mul_fixed32( RP.E, RP.yst + mul_fixed32( RP.dyst, vcnt << (16 - vcnt_shift)) - RP.py ) +
				mul_fixed32( RP.F, RP.zst - RP.pz );
		/*xp  = RP.A * ( RP.px - RP.cx ) + RP.B * ( RP.py - RP.cy ) + RP.C * ( RP.pz - RP.cz ) + RP.cx + RP.mx; */
		/*yp  = RP.D * ( RP.px - RP.cx ) + RP.E * ( RP.py - RP.cy ) + RP.F * ( RP.pz - RP.cz ) + RP.cy + RP.my; */
		/*dx  = (RP.A * RP.dx) + (RP.B * RP.dy); */
		/*dy  = (RP.D * RP.dx) + (RP.E * RP.dy); */

		line = BMP32(bitmap, vcnt);

		/* TODO: nuke this spaghetti code */
		if ( !use_coeff_table || RP.dkax == 0 )
		{
			if ( use_coeff_table )
			{
				switch( coeff_table_size )
				{
					case 0:
						address = coeff_table_offset + ((RP.kast + RP.dkast*(vcnt>>vcnt_shift)) >> 16) * 4;
						coeff_table_val = coeff_table_base[ address / 4 ];
						/*coeff_line_color_screen_data = (coeff_table_val & 0x7f000000) >> 24; */
						coeff_msb = (coeff_table_val & 0x80000000) > 0;
						if ( coeff_table_val & 0x00800000 )
						{
							coeff_table_val |= 0xff000000;
						}
						else
						{
							coeff_table_val &= 0x007fffff;
						}
						break;
					case 1:
						address = coeff_table_offset + ((RP.kast + RP.dkast*(vcnt>>vcnt_shift)) >> 16) * 2;
						coeff_table_val = coeff_table_base[ address / 4 ];
						if ( (address & 2) == 0 )
						{
							coeff_table_val >>= 16;
						}
						coeff_table_val &= 0xffff;
						/*coeff_line_color_screen_data = 0; */
						coeff_msb = (coeff_table_val & 0x8000) > 0;
						if ( coeff_table_val & 0x4000 )
						{
							coeff_table_val |= 0xffff8000;
						}
						else
						{
							coeff_table_val &= 0x3fff;
						}
						coeff_table_val <<= 6; /* to form 16.16 fixed point val */
						break;
					default:
						coeff_msb = 1;
						break;
				}
				if ( coeff_msb ) continue;

				switch( coeff_table_mode )
				{
					case 0:
						kx = ky = coeff_table_val;
						break;
					case 1:
						kx = coeff_table_val;
						break;
					case 2:
						ky = coeff_table_val;
						break;
					case 3:
						xp = coeff_table_val;
						break;
				}
			}

			/*x = RP.kx * ( xsp + dx * (hcnt << 16)) + xp; */
			/*y = RP.ky * ( ysp + dy * (hcnt << 16)) + yp; */
			xs = mul_fixed32( kx, xsp ) + xp;
			ys = mul_fixed32( ky, ysp ) + yp;
			dxs = mul_fixed32( kx, mul_fixed32( dx, 1 << (16-hcnt_shift)));
			dys = mul_fixed32( ky, mul_fixed32( dy, 1 << (16-hcnt_shift)));

			use_winmask = vdp2_roz_window_line(winmask, cliprect->min_x, cliprect->max_x, vcnt, current_tilemap.roz_mode3 == 1, iRP - 1);
			/* the layer settings, read once: the stores to the line could alias them */
			transparency = current_tilemap.transparency;
			fade = current_tilemap.fade_control & 1;
			alpha = current_tilemap.alpha;
			for (hcnt = cliprect->min_x; hcnt <= cliprect->max_x; xs+=dxs, ys+=dys, hcnt++ )
			{
#ifdef __GNUC__
				/* the plane is up to 64MB: fetch the pixel needed a few steps ahead */
				__builtin_prefetch(&rozbase[(((ys + dys * ROZ_PREFETCH) >> 16) & planerenderedsizey) * rozpitch + (((xs + dxs * ROZ_PREFETCH) >> 16) & planerenderedsizex)]);
#endif
				x = xs >> 16;
				y = ys >> 16;

				if ( x & clipxmask || y & clipymask ) continue;
				if ( use_winmask && !winmask[hcnt] )
					continue;

				pix = rozbase[(y & planerenderedsizey) * rozpitch + (x & planerenderedsizex)];
				if (transparency & STV_TRANSPARENCY_ALPHA)
				{
					if ((transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(fade)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = stv_alpha_blend_r32( line[hcnt], pix, alpha );
					}
				}
				else if (transparency & STV_TRANSPARENCY_ADD_BLEND)
				{
					if ((transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(fade)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = add_blend_r32( line[hcnt], pix );
					}
				}
				else
				{
					if ((transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(fade)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = pix;
					}
				}
			}
		}
		else
		{
			for (hcnt = cliprect->min_x; hcnt <= cliprect->max_x; hcnt++ )
			{
				switch( coeff_table_size )
				{
					case 0:
						address = coeff_table_offset + ((RP.kast + RP.dkast*(vcnt>>vcnt_shift) + RP.dkax*hcnt) >> 16) * 4;
						coeff_table_val = coeff_table_base[ address / 4 ];
						/*coeff_line_color_screen_data = (coeff_table_val & 0x7f000000) >> 24; */
						coeff_msb = (coeff_table_val & 0x80000000) > 0;
						if ( coeff_table_val & 0x00800000 )
						{
							coeff_table_val |= 0xff000000;
						}
						else
						{
							coeff_table_val &= 0x007fffff;
						}
						break;
					case 1:
						address = coeff_table_offset + ((RP.kast + RP.dkast*(vcnt>>vcnt_shift) + RP.dkax*hcnt) >> 16) * 2;
						coeff_table_val = coeff_table_base[ address / 4 ];
						if ( (address & 2) == 0 )
						{
							coeff_table_val >>= 16;
						}
						coeff_table_val &= 0xffff;
						/*coeff_line_color_screen_data = 0; */
						coeff_msb = (coeff_table_val & 0x8000) > 0;
						if ( coeff_table_val & 0x4000 )
						{
							coeff_table_val |= 0xffff8000;
						}
						else
						{
							coeff_table_val &= 0x3fff;
						}
						coeff_table_val <<= 6; /* to form 16.16 fixed point val */
						break;
					default:
						coeff_msb = 1;
						break;
				}
				if ( coeff_msb ) continue;
				switch( coeff_table_mode )
				{
					case 0:
						kx = ky = coeff_table_val;
						break;
					case 1:
						kx = coeff_table_val;
						break;
					case 2:
						ky = coeff_table_val;
						break;
					case 3:
						xp = coeff_table_val;
						break;
				}

				/*x = RP.kx * ( xsp + dx * (hcnt << 16)) + xp; */
				/*y = RP.ky * ( ysp + dy * (hcnt << 16)) + yp; */
				x = mul_fixed32( kx, xsp + mul_fixed32( dx, (hcnt>>hcnt_shift) << 16 ) ) + xp;
				y = mul_fixed32( ky, ysp + mul_fixed32( dy, (hcnt>>hcnt_shift) << 16 ) ) + yp;

				x >>= 16;
				y >>= 16;

				if ( x & clipxmask || y & clipymask ) continue;

				pix = rozbase[(y & planerenderedsizey) * rozpitch + (x & planerenderedsizex)];
				if (current_tilemap.transparency & STV_TRANSPARENCY_ALPHA)
				{
					if ((current_tilemap.transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(current_tilemap.fade_control & 1)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = stv_alpha_blend_r32( line[hcnt], pix, current_tilemap.alpha );
					}
				}
				else if (current_tilemap.transparency & STV_TRANSPARENCY_ADD_BLEND)
				{
					if ((current_tilemap.transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(current_tilemap.fade_control & 1)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = add_blend_r32( line[hcnt], pix );
					}
				}
				else
				{
					if ((current_tilemap.transparency & STV_TRANSPARENCY_NONE) || (pix & 0xffffff))
					{
						if(current_tilemap.fade_control & 1)
							pix = vdp2_apply_color_offset(pix, co_r, co_g, co_b);

						line[hcnt] = pix;
					}
				}
			}
		}
	}
}

static INLINE int get_roz_window_pixel(int s_x,int e_x,int s_y,int e_y,int x, int y,UINT8 winenable, UINT8 winarea)
{
	int res;

	res = 1;
	if(winenable)
	{
		if(winarea)
			res = (y >= s_y && y <= e_y && x >= s_x && x <= e_x);
		else
			res = (y >= s_y && y <= e_y && x >= s_x && x <= e_x) ^ 1;
	}

	return res;
}


static void vdp2_draw_NBG0(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT32 base_mask;

	base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;

	/*
	   Colours           : 16, 256, 2048, 32768, 16770000
	   Char Size         : 1x1 cells, 2x2 cells
	   Pattern Data Size : 1 word, 2 words
	   Plane Layouts     : 1 x 1, 2 x 1, 2 x 2
	   Planes            : 4
	   Bitmap            : Possible
	   Bitmap Sizes      : 512 x 256, 512 x 512, 1024 x 256, 1024 x 512
	   Scale             : 0.25 x - 256 x
	   Rotation          : No
	   Linescroll        : Yes
	   Column Scroll     : Yes
	   Mosaic            : Yes
	*/

	current_tilemap.enabled = VDP2_N0ON | VDP2_R1ON;

/*  if (!current_tilemap.enabled) return; // stop right now if its disabled ... */

	/*current_tilemap.trans_enabled = VDP2_N0TPON; */
	if ( VDP2_N0CCEN )
	{
		current_tilemap.colour_calculation_enabled = 1;
		current_tilemap.alpha = ((UINT16)(0x1f-VDP2_N0CCRT)*0xff)/0x1f;
	}
	else
	{
		current_tilemap.colour_calculation_enabled = 0;
	}
	if ( VDP2_N0TPON == 0 )
	{
		current_tilemap.transparency = STV_TRANSPARENCY_PEN;
	}
	else
	{
		current_tilemap.transparency = STV_TRANSPARENCY_NONE;
	}
	current_tilemap.colour_depth = VDP2_N0CHCN;
	current_tilemap.tile_size = VDP2_N0CHSZ;
	current_tilemap.bitmap_enable = VDP2_N0BMEN;
	current_tilemap.bitmap_size = VDP2_N0BMSZ;
	current_tilemap.bitmap_palette_number = VDP2_N0BMP;
	current_tilemap.bitmap_map = VDP2_N0MP_;
	current_tilemap.map_offset[0] = VDP2_N0MPA | (VDP2_N0MP_ << 6);
	current_tilemap.map_offset[1] = VDP2_N0MPB | (VDP2_N0MP_ << 6);
	current_tilemap.map_offset[2] = VDP2_N0MPC | (VDP2_N0MP_ << 6);
	current_tilemap.map_offset[3] = VDP2_N0MPD | (VDP2_N0MP_ << 6);
	current_tilemap.map_count = 4;

	current_tilemap.pattern_data_size = VDP2_N0PNB;
	current_tilemap.character_number_supplement = VDP2_N0CNSM;
	current_tilemap.special_priority_register = VDP2_N0SPR;
	current_tilemap.special_colour_control_register = VDP2_PNCN0;
	current_tilemap.supplementary_palette_bits = VDP2_N0SPLT;
	current_tilemap.supplementary_character_bits = VDP2_N0SPCN;

	current_tilemap.scrollx = VDP2_SCXIN0;
	current_tilemap.scrolly = VDP2_SCYIN0;
	current_tilemap.incx = VDP2_ZMXN0;
	current_tilemap.incy = VDP2_ZMYN0;

	current_tilemap.linescroll_enable = VDP2_N0LSCX;
	current_tilemap.linescroll_interval = ((get_lsmd() == 2) ? (2) : (1)) << (VDP2_N0LSS);
	current_tilemap.linescroll_table_address = (((VDP2_LSTA0U << 16) | VDP2_LSTA0L) & base_mask) * 2;
	current_tilemap.vertical_linescroll_enable = VDP2_N0LSCY;
	current_tilemap.linezoom_enable = VDP2_N0LZMX;
	current_tilemap.vertical_cell_scroll_enable = VDP2_N0VCSC;

	current_tilemap.plane_size = (VDP2_R1ON) ? VDP2_RBPLSZ : VDP2_N0PLSZ;
	current_tilemap.colour_ram_address_offset = VDP2_N0CAOS;
	current_tilemap.fade_control = (VDP2_N0COEN * 1) | (VDP2_N0COSL * 2);
	vdp2_check_fade_control_for_layer();
	current_tilemap.window_control.logic = VDP2_N0LOG;
	current_tilemap.window_control.enabled[0] = VDP2_N0W0E;
	current_tilemap.window_control.enabled[1] = VDP2_N0W1E;
/*  current_tilemap.window_control.? = VDP2_N0SWE; */
	current_tilemap.window_control.area[0] = VDP2_N0W0A;
	current_tilemap.window_control.area[1] = VDP2_N0W1A;
/*  current_tilemap.window_control.? = VDP2_N0SWA; */

	current_tilemap.line_screen_enabled = VDP2_N0LCEN;
	current_tilemap.mosaic_screen_enabled = VDP2_N0MZE;

	current_tilemap.layer_name=(VDP2_R1ON) ? 0x81 : 0;

	if ( current_tilemap.enabled && (!(VDP2_R1ON))) /* TODO: check cycle pattern for RBG1 */
	{
		current_tilemap.enabled = vdp2_check_vram_cycle_pattern_registers( VDP2_CP_NBG0_PNMDR, VDP2_CP_NBG0_CPDR, current_tilemap.bitmap_enable );
	}

	if(VDP2_R1ON)
		vdp2_draw_rotation_screen(bitmap, cliprect, 2 );
	else
		vdp2_check_tilemap(bitmap, cliprect);
}

static void vdp2_draw_NBG1(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	UINT32 base_mask;

	base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;

	/*
	   Colours           : 16, 256, 2048, 32768
	   Char Size         : 1x1 cells, 2x2 cells
	   Pattern Data Size : 1 word, 2 words
	   Plane Layouts     : 1 x 1, 2 x 1, 2 x 2
	   Planes            : 4
	   Bitmap            : Possible
	   Bitmap Sizes      : 512 x 256, 512 x 512, 1024 x 256, 1024 x 512
	   Scale             : 0.25 x - 256 x
	   Rotation          : No
	   Linescroll        : Yes
	   Column Scroll     : Yes
	   Mosaic            : Yes
	*/
	current_tilemap.enabled = VDP2_N1ON;

/*  if (!current_tilemap.enabled) return; // stop right now if its disabled ... */

	/*current_tilemap.trans_enabled = VDP2_N1TPON; */
	if ( VDP2_N1CCEN )
	{
		current_tilemap.colour_calculation_enabled = 1;
		current_tilemap.alpha = ((UINT16)(0x1f-VDP2_N1CCRT)*0xff)/0x1f;
	}
	else
	{
		current_tilemap.colour_calculation_enabled = 0;
	}
	if ( VDP2_N1TPON == 0 )
	{
		current_tilemap.transparency = STV_TRANSPARENCY_PEN;
	}
	else
	{
		current_tilemap.transparency = STV_TRANSPARENCY_NONE;
	}
	current_tilemap.colour_depth = VDP2_N1CHCN;
	current_tilemap.tile_size = VDP2_N1CHSZ;
	current_tilemap.bitmap_enable = VDP2_N1BMEN;
	current_tilemap.bitmap_size = VDP2_N1BMSZ;
	current_tilemap.bitmap_palette_number = VDP2_N1BMP;
	current_tilemap.bitmap_map = VDP2_N1MP_;
	current_tilemap.map_offset[0] = VDP2_N1MPA | (VDP2_N1MP_ << 6);
	current_tilemap.map_offset[1] = VDP2_N1MPB | (VDP2_N1MP_ << 6);
	current_tilemap.map_offset[2] = VDP2_N1MPC | (VDP2_N1MP_ << 6);
	current_tilemap.map_offset[3] = VDP2_N1MPD | (VDP2_N1MP_ << 6);
	current_tilemap.map_count = 4;

	current_tilemap.pattern_data_size = VDP2_N1PNB;
	current_tilemap.character_number_supplement = VDP2_N1CNSM;
	current_tilemap.special_priority_register = VDP2_N1SPR;
	current_tilemap.special_colour_control_register = VDP2_PNCN1;
	current_tilemap.supplementary_palette_bits = VDP2_N1SPLT;
	current_tilemap.supplementary_character_bits = VDP2_N1SPCN;

	current_tilemap.scrollx = VDP2_SCXIN1;
	current_tilemap.scrolly = VDP2_SCYIN1;
	current_tilemap.incx = VDP2_ZMXN1;
	current_tilemap.incy = VDP2_ZMYN1;

	current_tilemap.linescroll_enable = VDP2_N1LSCX;
	current_tilemap.linescroll_interval = ((get_lsmd() == 2) ? (2) : (1)) << (VDP2_N1LSS);
	current_tilemap.linescroll_table_address = (((VDP2_LSTA1U << 16) | VDP2_LSTA1L) & base_mask) * 2;
	current_tilemap.vertical_linescroll_enable = VDP2_N1LSCY;
	current_tilemap.linezoom_enable = VDP2_N1LZMX;
	current_tilemap.vertical_cell_scroll_enable = VDP2_N1VCSC;

	current_tilemap.plane_size = VDP2_N1PLSZ;
	current_tilemap.colour_ram_address_offset = VDP2_N1CAOS;
	current_tilemap.fade_control = (VDP2_N1COEN * 1) | (VDP2_N1COSL * 2);
	vdp2_check_fade_control_for_layer();
	current_tilemap.window_control.logic = VDP2_N1LOG;
	current_tilemap.window_control.enabled[0] = VDP2_N1W0E;
	current_tilemap.window_control.enabled[1] = VDP2_N1W1E;
/*  current_tilemap.window_control.? = VDP2_N1SWE; */
	current_tilemap.window_control.area[0] = VDP2_N1W0A;
	current_tilemap.window_control.area[1] = VDP2_N1W1A;
/*  current_tilemap.window_control.? = VDP2_N1SWA; */

	current_tilemap.line_screen_enabled = VDP2_N1LCEN;
	current_tilemap.mosaic_screen_enabled = VDP2_N1MZE;

	current_tilemap.layer_name=1;

	if ( current_tilemap.enabled )
	{
		current_tilemap.enabled = vdp2_check_vram_cycle_pattern_registers( VDP2_CP_NBG1_PNMDR, VDP2_CP_NBG1_CPDR, current_tilemap.bitmap_enable );
	}

	vdp2_check_tilemap(bitmap, cliprect);
}

static void vdp2_draw_NBG2(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/*
	   NBG2 is the first of the 2 more basic tilemaps, it has exactly the same capabilities as NBG3

	   Colours           : 16, 256
	   Char Size         : 1x1 cells, 2x2 cells
	   Pattern Data Size : 1 word, 2 words
	   Plane Layouts     : 1 x 1, 2 x 1, 2 x 2
	   Planes            : 4
	   Bitmap            : No
	   Bitmap Sizes      : N/A
	   Scale             : No
	   Rotation          : No
	   Linescroll        : No
	   Column Scroll     : No
	   Mosaic            : Yes
	*/

	current_tilemap.enabled = VDP2_N2ON;

	/* these modes for N0 disable this layer */
	if (VDP2_N0CHCN == 0x03) current_tilemap.enabled = 0;
	if (VDP2_N0CHCN == 0x04) current_tilemap.enabled = 0;

/*  if (!current_tilemap.enabled) return; // stop right now if its disabled ... */

	/*current_tilemap.trans_enabled = VDP2_N2TPON; */
	if ( VDP2_N2CCEN )
	{
		current_tilemap.colour_calculation_enabled = 1;
		current_tilemap.alpha = ((UINT16)(0x1f-VDP2_N2CCRT)*0xff)/0x1f;
	}
	else
	{
		current_tilemap.colour_calculation_enabled = 0;
	}
	if ( VDP2_N2TPON == 0 )
	{
		current_tilemap.transparency = STV_TRANSPARENCY_PEN;
	}
	else
	{
		current_tilemap.transparency = STV_TRANSPARENCY_NONE;
	}
	current_tilemap.colour_depth = VDP2_N2CHCN;
	current_tilemap.tile_size = VDP2_N2CHSZ;
	/* this layer can't be a bitmap,so ignore these registers*/
	current_tilemap.bitmap_enable = 0;
	current_tilemap.bitmap_size = 0;
	current_tilemap.bitmap_palette_number = 0;
	current_tilemap.bitmap_map = 0;
	current_tilemap.map_offset[0] = VDP2_N2MPA | (VDP2_N2MP_ << 6);
	current_tilemap.map_offset[1] = VDP2_N2MPB | (VDP2_N2MP_ << 6);
	current_tilemap.map_offset[2] = VDP2_N2MPC | (VDP2_N2MP_ << 6);
	current_tilemap.map_offset[3] = VDP2_N2MPD | (VDP2_N2MP_ << 6);
	current_tilemap.map_count = 4;

	current_tilemap.pattern_data_size = VDP2_N2PNB;
	current_tilemap.character_number_supplement = VDP2_N2CNSM;
	current_tilemap.special_priority_register = VDP2_N2SPR;
	current_tilemap.special_colour_control_register = VDP2_PNCN2;
	current_tilemap.supplementary_palette_bits = VDP2_N2SPLT;
	current_tilemap.supplementary_character_bits = VDP2_N2SPCN;

	current_tilemap.scrollx = VDP2_SCXN2;
	current_tilemap.scrolly = VDP2_SCYN2;
	/*This layer can't be scaled*/
	current_tilemap.incx = 0x10000;
	current_tilemap.incy = 0x10000;

	current_tilemap.linescroll_enable = 0;
	current_tilemap.linescroll_interval = 0;
	current_tilemap.linescroll_table_address = 0;
	current_tilemap.vertical_linescroll_enable = 0;
	current_tilemap.linezoom_enable = 0;
	current_tilemap.vertical_cell_scroll_enable = 0;

	current_tilemap.colour_ram_address_offset = VDP2_N2CAOS;
	current_tilemap.fade_control = (VDP2_N2COEN * 1) | (VDP2_N2COSL * 2);
	vdp2_check_fade_control_for_layer();
	current_tilemap.window_control.logic = VDP2_N2LOG;
	current_tilemap.window_control.enabled[0] = VDP2_N2W0E;
	current_tilemap.window_control.enabled[1] = VDP2_N2W1E;
/*  current_tilemap.window_control.? = VDP2_N2SWE; */
	current_tilemap.window_control.area[0] = VDP2_N2W0A;
	current_tilemap.window_control.area[1] = VDP2_N2W1A;
/*  current_tilemap.window_control.? = VDP2_N2SWA; */

	current_tilemap.line_screen_enabled = VDP2_N2LCEN;
	current_tilemap.mosaic_screen_enabled = VDP2_N2MZE;

	current_tilemap.layer_name=2;

	current_tilemap.plane_size = VDP2_N2PLSZ;

	if ( current_tilemap.enabled )
	{
		current_tilemap.enabled = vdp2_check_vram_cycle_pattern_registers( VDP2_CP_NBG2_PNMDR, VDP2_CP_NBG2_CPDR, current_tilemap.bitmap_enable );
	}

	vdp2_check_tilemap(bitmap, cliprect);
}

static void vdp2_draw_NBG3(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/*
	   NBG3 is the second of the 2 more basic tilemaps, it has exactly the same capabilities as NBG2

	   Colours           : 16, 256
	   Char Size         : 1x1 cells, 2x2 cells
	   Pattern Data Size : 1 word, 2 words
	   Plane Layouts     : 1 x 1, 2 x 1, 2 x 2
	   Planes            : 4
	   Bitmap            : No
	   Bitmap Sizes      : N/A
	   Scale             : No
	   Rotation          : No
	   Linescroll        : No
	   Column Scroll     : No
	   Mosaic            : Yes
	*/

	current_tilemap.enabled = VDP2_N3ON;

/*  if (!current_tilemap.enabled) return; // stop right now if its disabled ... */

	/* these modes for N1 disable this layer */
	if (VDP2_N1CHCN == 0x03) current_tilemap.enabled = 0;
	if (VDP2_N1CHCN == 0x04) current_tilemap.enabled = 0;

	/*current_tilemap.trans_enabled = VDP2_N3TPON; */
	if ( VDP2_N3CCEN )
	{
		current_tilemap.colour_calculation_enabled = 1;
		current_tilemap.alpha = ((UINT16)(0x1f-VDP2_N3CCRT)*0xff)/0x1f;
	}
	else
	{
		current_tilemap.colour_calculation_enabled = 0;
	}
	if ( VDP2_N3TPON == 0 )
	{
		current_tilemap.transparency = STV_TRANSPARENCY_PEN;
	}
	else
	{
		current_tilemap.transparency = STV_TRANSPARENCY_NONE;
	}
	current_tilemap.colour_depth = VDP2_N3CHCN;
	current_tilemap.tile_size = VDP2_N3CHSZ;
	/* this layer can't be a bitmap,so ignore these registers*/
	current_tilemap.bitmap_enable = 0;
	current_tilemap.bitmap_size = 0;
	current_tilemap.bitmap_palette_number = 0;
	current_tilemap.bitmap_map = 0;
	current_tilemap.map_offset[0] = VDP2_N3MPA | (VDP2_N3MP_ << 6);
	current_tilemap.map_offset[1] = VDP2_N3MPB | (VDP2_N3MP_ << 6);
	current_tilemap.map_offset[2] = VDP2_N3MPC | (VDP2_N3MP_ << 6);
	current_tilemap.map_offset[3] = VDP2_N3MPD | (VDP2_N3MP_ << 6);
	current_tilemap.map_count = 4;

	current_tilemap.pattern_data_size = VDP2_N3PNB;
	current_tilemap.character_number_supplement = VDP2_N3CNSM;
	current_tilemap.special_priority_register = VDP2_N3SPR;
	current_tilemap.special_colour_control_register = VDP2_N3SCC;
	current_tilemap.supplementary_palette_bits = VDP2_N3SPLT;
	current_tilemap.supplementary_character_bits = VDP2_N3SPCN;

	current_tilemap.scrollx = VDP2_SCXN3;
	current_tilemap.scrolly = VDP2_SCYN3;
	/*This layer can't be scaled*/
	current_tilemap.incx = 0x10000;
	current_tilemap.incy = 0x10000;

	current_tilemap.linescroll_enable = 0;
	current_tilemap.linescroll_interval = 0;
	current_tilemap.linescroll_table_address = 0;
	current_tilemap.vertical_linescroll_enable = 0;
	current_tilemap.linezoom_enable = 0;
	current_tilemap.vertical_cell_scroll_enable = 0;

	current_tilemap.colour_ram_address_offset = VDP2_N3CAOS;
	current_tilemap.fade_control = (VDP2_N3COEN * 1) | (VDP2_N3COSL * 2);
	vdp2_check_fade_control_for_layer();
	current_tilemap.window_control.logic = VDP2_N3LOG;
	current_tilemap.window_control.enabled[0] = VDP2_N3W0E;
	current_tilemap.window_control.enabled[1] = VDP2_N3W1E;
/*  current_tilemap.window_control.? = VDP2_N3SWE; */
	current_tilemap.window_control.area[0] = VDP2_N3W0A;
	current_tilemap.window_control.area[1] = VDP2_N3W1A;
/*  current_tilemap.window_control.? = VDP2_N3SWA; */

	current_tilemap.line_screen_enabled = VDP2_N3LCEN;
	current_tilemap.mosaic_screen_enabled = VDP2_N3MZE;

	current_tilemap.layer_name=3;

	current_tilemap.plane_size = VDP2_N3PLSZ;

	if ( current_tilemap.enabled )
	{
		current_tilemap.enabled = vdp2_check_vram_cycle_pattern_registers( VDP2_CP_NBG3_PNMDR, VDP2_CP_NBG3_CPDR, current_tilemap.bitmap_enable );
	}

	vdp2_check_tilemap(bitmap, cliprect);
}


static void vdp2_draw_rotation_screen(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int iRP)
{
	int planesizex = 0, planesizey = 0;
	struct rectangle mycliprect;

	if (iRP == 1)
	{
		current_tilemap.bitmap_map = VDP2_RAMP_;
		current_tilemap.map_offset[0] = VDP2_RAMPA | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[1] = VDP2_RAMPB | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[2] = VDP2_RAMPC | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[3] = VDP2_RAMPD | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[4] = VDP2_RAMPE | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[5] = VDP2_RAMPF | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[6] = VDP2_RAMPG | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[7] = VDP2_RAMPH | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[8] = VDP2_RAMPI | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[9] = VDP2_RAMPJ | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[10] = VDP2_RAMPK | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[11] = VDP2_RAMPL | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[12] = VDP2_RAMPM | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[13] = VDP2_RAMPN | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[14] = VDP2_RAMPO | (VDP2_RAMP_ << 6);
		current_tilemap.map_offset[15] = VDP2_RAMPP | (VDP2_RAMP_ << 6);
		current_tilemap.map_count = 16;
	}
	else
	{
		current_tilemap.bitmap_map = VDP2_RBMP_;
		current_tilemap.map_offset[0] = VDP2_RBMPA | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[1] = VDP2_RBMPB | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[2] = VDP2_RBMPC | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[3] = VDP2_RBMPD | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[4] = VDP2_RBMPE | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[5] = VDP2_RBMPF | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[6] = VDP2_RBMPG | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[7] = VDP2_RBMPH | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[8] = VDP2_RBMPI | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[9] = VDP2_RBMPJ | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[10] = VDP2_RBMPK | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[11] = VDP2_RBMPL | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[12] = VDP2_RBMPM | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[13] = VDP2_RBMPN | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[14] = VDP2_RBMPO | (VDP2_RBMP_ << 6);
		current_tilemap.map_offset[15] = VDP2_RBMPP | (VDP2_RBMP_ << 6);
		current_tilemap.map_count = 16;
	}

	vdp2_fill_rotation_parameter_table(iRP);

	if ( iRP == 1 )
	{
		current_tilemap.plane_size = VDP2_RAPLSZ;
	}
	else
	{
		current_tilemap.plane_size = VDP2_RBPLSZ;
	}

	if (current_tilemap.bitmap_enable)
	{
		switch (current_tilemap.bitmap_size)
		{
			case 0: planesizex=512; planesizey=256; break;
			case 1: planesizex=512; planesizey=512; break;
			case 2: planesizex=1024; planesizey=256; break;
			case 3: planesizex=1024; planesizey=512; break;
		}
	}
	else
	{
		switch( current_tilemap.plane_size )
		{
		case 0:
			planesizex = planesizey = 2048;
			break;
		case 1:
			planesizex = 4096;
			planesizey = 2048;
			break;
		case 2:
			planesizex = 0;
			planesizey = 0;
			break;
		case 3:
			planesizex = planesizey = 4096;
			break;
		}
	}

	if ( vdp2_is_rotation_applied() == 0 )
	{
		current_tilemap.scrollx = current_rotation_table.mx >> 16;
		current_tilemap.scrolly = current_rotation_table.my >> 16;

		if(current_tilemap.roz_mode3 == 1)
		{
			/* TODO: Cotton 2 enables mode 3 without an actual RP window enabled */
			/*       Technically you could use split screen effect without rotation applied, */
			/*       which will be annoying to emulate with this video structure. */
			/*       Let's see if anything will do it ... */
			if(VDP2_RPW0E || VDP2_RPW1E)
				popmessage("ROZ Mode 3 window enabled without zooming");

			if(iRP == 2)
				return;
		}

		/* TODO: legacy code, to be removed */
		current_tilemap.window_control.logic = VDP2_R0LOG;
		current_tilemap.window_control.enabled[0] = VDP2_R0W0E;
		current_tilemap.window_control.enabled[1] = VDP2_R0W1E;
/*      current_tilemap.window_control.? = VDP2_R0SWE; */
		current_tilemap.window_control.area[0] = VDP2_R0W0A;
		current_tilemap.window_control.area[1] = VDP2_R0W1A;
/*      current_tilemap.window_control.? = VDP2_R0SWA; */

		mycliprect = *cliprect;

		if ( current_tilemap.window_control.enabled[0] || current_tilemap.window_control.enabled[1] )
		{
			/*popmessage("Window control for RBG"); */
			vdp2_apply_window_on_layer(&mycliprect);
			current_tilemap.window_control.enabled[0] = 0;
			current_tilemap.window_control.enabled[1] = 0;
		}

		vdp2_check_tilemap(bitmap,&mycliprect);
	}
	else
	{
		struct rectangle roz_clip_rect;
		int planerenderedsizex, planerenderedsizey;
		UINT8 colour_calculation_enabled, fade_control;

		/* the rotation plane cache, allocated on first use (64MB each) */
		if ( !vdp2l.roz_bitmap[iRP-1] )
			vdp2l.roz_bitmap[iRP-1] = bitmap_alloc_depth(4096, 4096, 32);

		roz_clip_rect.min_x = roz_clip_rect.min_y = 0;
		if ( (iRP == 1 && VDP2_RAOVR == 3) ||
				(iRP == 2 && VDP2_RBOVR == 3) )
		{
			roz_clip_rect.max_x = roz_clip_rect.max_y = 511;
			planerenderedsizex = planerenderedsizey = 512;
		}
		else if (vdp2_are_map_registers_equal() &&
					!current_tilemap.bitmap_enable)
		{
			roz_clip_rect.max_x = (planesizex / 4) - 1;
			roz_clip_rect.max_y = (planesizey / 4) - 1;
			planerenderedsizex = planesizex / 4;
			planerenderedsizey = planesizey / 4;
		}
		else
		{
			roz_clip_rect.max_x = planesizex - 1;
			roz_clip_rect.max_y = planesizey - 1;
			planerenderedsizex = planesizex;
			planerenderedsizey = planesizey;
		}


		colour_calculation_enabled = current_tilemap.colour_calculation_enabled;
		current_tilemap.colour_calculation_enabled = 0;
/*      window_control = current_tilemap.window_control; */
/*      current_tilemap.window_control = 0; */
		fade_control = current_tilemap.fade_control;
		current_tilemap.fade_control = 0;
		/* the bands only read the caches the prepass updated */
		if (vdp2_render_mode != VDP2_RENDER_BAND)
		{
			const int same_layer = !(RBG0_cache_data.is_cache_dirty & iRP) &&
				memcmp(&RBG0_cache_data.layer_data[iRP-1],&current_tilemap,sizeof(current_tilemap)) == 0;
			const int map_dirty = RBG0_cache_data.map_dirty_max[iRP-1] > RBG0_cache_data.map_dirty_min[iRP-1];

			if ( same_layer && map_dirty && !current_tilemap.bitmap_enable &&
				!current_tilemap.linescroll_enable && !current_tilemap.vertical_linescroll_enable &&
				!current_tilemap.linezoom_enable &&
				current_tilemap.incx == 0x10000 && current_tilemap.incy == 0x10000 &&
				!(current_tilemap.transparency & STV_TRANSPARENCY_ADD_BLEND) )
			{
				/* - diehard rewrites a few pattern names of its RBG0 map every frame */
				vdp2_cell_redraw.active = 1;
				vdp2_cell_redraw.min = RBG0_cache_data.map_dirty_min[iRP-1];
				vdp2_cell_redraw.max = RBG0_cache_data.map_dirty_max[iRP-1];
				vdp2_check_tilemap(vdp2l.roz_bitmap[iRP-1], &roz_clip_rect);
				vdp2_cell_redraw.active = 0;

				/* the redrawn cells may use more characters and colours than the cache watches */
				if ( vdp2_layer_data.tile_offset_max > vdp2_layer_data.tile_offset_min )
				{
					if ( vdp2_layer_data.tile_offset_min < RBG0_cache_data.tile_offset_min[iRP-1] )
						RBG0_cache_data.tile_offset_min[iRP-1] = vdp2_layer_data.tile_offset_min;
					if ( vdp2_layer_data.tile_offset_max > RBG0_cache_data.tile_offset_max[iRP-1] )
						RBG0_cache_data.tile_offset_max[iRP-1] = vdp2_layer_data.tile_offset_max;
				}
				if ( vdp2_layer_data.pen_max > 0x800 )
				{
					vdp2_layer_data.pen_min = 0;
					vdp2_layer_data.pen_max = 0x800;
				}
				if ( vdp2_layer_data.pen_max > vdp2_layer_data.pen_min )
				{
					if ( RBG0_cache_data.pen_max[iRP-1] <= RBG0_cache_data.pen_min[iRP-1] )
					{
						RBG0_cache_data.pen_min[iRP-1] = vdp2_layer_data.pen_min;
						RBG0_cache_data.pen_max[iRP-1] = vdp2_layer_data.pen_max;
					}
					else
					{
						if ( vdp2_layer_data.pen_min < RBG0_cache_data.pen_min[iRP-1] )
							RBG0_cache_data.pen_min[iRP-1] = vdp2_layer_data.pen_min;
						if ( vdp2_layer_data.pen_max > RBG0_cache_data.pen_max[iRP-1] )
							RBG0_cache_data.pen_max[iRP-1] = vdp2_layer_data.pen_max;
					}
				}
				RBG0_cache_data.map_dirty_min[iRP-1] = RBG0_cache_data.map_dirty_max[iRP-1] = 0;
			}
			else if ( !same_layer || map_dirty )
			{
				RBG0_cache_data.map_dirty_min[iRP-1] = RBG0_cache_data.map_dirty_max[iRP-1] = 0;
				bitmap_fill32(vdp2l.roz_bitmap[iRP-1], &roz_clip_rect, 0);
				vdp2_check_tilemap(vdp2l.roz_bitmap[iRP-1], &roz_clip_rect);
				if ( current_tilemap.bitmap_enable )
				{
					/* a bitmap is read straight out of VRAM, so the tilemap path didn't set up the ranges to watch */
					/* - vkyoute2 water kept stale contents */
					static const int bits_per_dot[8] = { 4, 8, 16, 16, 32, 32, 32, 32 };
					static const UINT32 colours[8] = { 16, 256, 0x800, 0, 0, 0, 0, 0 };
					const UINT32 dots = ((current_tilemap.bitmap_size & 2) ? 1024 : 512) * ((current_tilemap.bitmap_size & 1) ? 512 : 256);
					const UINT32 start = (current_tilemap.bitmap_map * 0x20000 / 4) & 0x1ffff;
					const UINT32 end = start + dots * bits_per_dot[current_tilemap.colour_depth & 7] / 32;
					UINT32 pal_bank;

					/* watch all of VRAM if the bitmap wraps around its end */
					vdp2_layer_data.map_offset_min = (end > 0x20000) ? 0 : start;
					vdp2_layer_data.map_offset_max = (end > 0x20000) ? 0x20000 : end;
					vdp2_layer_data.tile_offset_min = 0;
					vdp2_layer_data.tile_offset_max = 0;

					/* same palette selection as the draw_*_bitmap functions, fading is off while caching */
					pal_bank = (current_tilemap.colour_depth == 2) ? 0 : (((current_tilemap.bitmap_palette_number + current_tilemap.colour_ram_address_offset) & 7) << 8);
					vdp2_layer_data.pen_min = pal_bank;
					vdp2_layer_data.pen_max = pal_bank + colours[current_tilemap.colour_depth & 7];
				}
				/* the colour RAM address wraps, watch all of it if the range goes past its end */
				if ( vdp2_layer_data.pen_max > 0x800 )
				{
					vdp2_layer_data.pen_min = 0;
					vdp2_layer_data.pen_max = 0x800;
				}
				/* prepare cache data */
				RBG0_cache_data.watch_vdp2_vram_writes |= iRP;
				RBG0_cache_data.is_cache_dirty &= ~iRP;
				memcpy(&RBG0_cache_data.layer_data[iRP-1], &current_tilemap, sizeof(current_tilemap));
				RBG0_cache_data.map_offset_min[iRP-1] = vdp2_layer_data.map_offset_min;
				RBG0_cache_data.map_offset_max[iRP-1] = vdp2_layer_data.map_offset_max;
				RBG0_cache_data.tile_offset_min[iRP-1] = vdp2_layer_data.tile_offset_min;
				RBG0_cache_data.tile_offset_max[iRP-1] = vdp2_layer_data.tile_offset_max;
				RBG0_cache_data.pen_min[iRP-1] = vdp2_layer_data.pen_min;
				RBG0_cache_data.pen_max[iRP-1] = vdp2_layer_data.pen_max;
				log_masked(LOG_VDP2, "Cache watch: map = %06X - %06X, tile = %06X - %06X\n", RBG0_cache_data.map_offset_min[iRP-1],
					RBG0_cache_data.map_offset_max[iRP-1], RBG0_cache_data.tile_offset_min[iRP-1], RBG0_cache_data.tile_offset_max[iRP-1]);
			}
			/* stop profiling USER1 */
		}

		current_tilemap.colour_calculation_enabled = colour_calculation_enabled;
		if ( colour_calculation_enabled )
		{
			current_tilemap.transparency |= STV_TRANSPARENCY_ALPHA;
		}

#if 0
		/* old reference code */
		mycliprect = cliprect;

		if ( current_tilemap.window_control.enabled[0] || current_tilemap.window_control.enabled[1] )
		{
			/*popmessage("Window control for RBG"); */
			vdp2_apply_window_on_layer(mycliprect);
			current_tilemap.window_control.enabled[0] = 0;
			current_tilemap.window_control.enabled[1] = 0;
		}
#endif

		current_tilemap.fade_control = fade_control;

		if (vdp2_render_mode != VDP2_RENDER_PREPASS)
			vdp2_copy_roz_bitmap(bitmap, vdp2l.roz_bitmap[iRP-1], cliprect, iRP, planesizex, planesizey, planerenderedsizex, planerenderedsizey );
	}

}

static void vdp2_draw_RBG0(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/*
	   Colours           : 16, 256, 2048, 32768, 16770000
	   Char Size         : 1x1 cells, 2x2 cells
	   Pattern Data Size : 1 word, 2 words
	   Plane Layouts     : 1 x 1, 2 x 1, 2 x 2
	   Planes            : 4
	   Bitmap            : Possible
	   Bitmap Sizes      : 512 x 256, 512 x 512, 1024 x 256, 1024 x 512
	   Scale             : 0.25 x - 256 x
	   Rotation          : Yes
	   Linescroll        : Yes
	   Column Scroll     : Yes
	   Mosaic            : Yes
	*/

	current_tilemap.enabled = VDP2_R0ON;

/*  if (!current_tilemap.enabled) return; // stop right now if its disabled ... */

	/*current_tilemap.trans_enabled = VDP2_R0TPON; */
	if ( VDP2_R0CCEN )
	{
		current_tilemap.colour_calculation_enabled = 1;
		current_tilemap.alpha = ((UINT16)(0x1f-VDP2_R0CCRT)*0xff)/0x1f;
	}
	else
	{
		current_tilemap.colour_calculation_enabled = 0;
	}
	if ( VDP2_R0TPON == 0 )
	{
		current_tilemap.transparency = STV_TRANSPARENCY_PEN;
	}
	else
	{
		current_tilemap.transparency = STV_TRANSPARENCY_NONE;
	}
	current_tilemap.colour_depth = VDP2_R0CHCN;
	current_tilemap.tile_size = VDP2_R0CHSZ;
	current_tilemap.bitmap_enable = VDP2_R0BMEN;
	current_tilemap.bitmap_size = VDP2_R0BMSZ;
	current_tilemap.bitmap_palette_number = VDP2_R0BMP;

	current_tilemap.pattern_data_size = VDP2_R0PNB;
	current_tilemap.character_number_supplement = VDP2_R0CNSM;
	current_tilemap.special_priority_register = VDP2_R0SPR;
	current_tilemap.special_colour_control_register = VDP2_R0SCC;
	current_tilemap.supplementary_palette_bits = VDP2_R0SPLT;
	current_tilemap.supplementary_character_bits = VDP2_R0SPCN;

	current_tilemap.colour_ram_address_offset = VDP2_R0CAOS;
	current_tilemap.fade_control = (VDP2_R0COEN * 1) | (VDP2_R0COSL * 2);
	vdp2_check_fade_control_for_layer();
	/* disable these, we apply them in the roz routines (they were interfering with vdp2_roz_window() ?) */
	current_tilemap.window_control.logic = 0; /*VDP2_R0LOG; */
	current_tilemap.window_control.enabled[0] = 0; /*VDP2_R0W0E; */
	current_tilemap.window_control.enabled[1] = 0; /*VDP2_R0W1E; */
/*  current_tilemap.window_control.? = VDP2_R0SWE; */
	current_tilemap.window_control.area[0] = 0; /*VDP2_R0W0A; */
	current_tilemap.window_control.area[1] = 0; /*VDP2_R0W1A; */
/*  current_tilemap.window_control.? = VDP2_R0SWA; */

	current_tilemap.scrollx = 0;
	current_tilemap.scrolly = 0;
	current_tilemap.incx = 0x10000;
	current_tilemap.incy = 0x10000;

	current_tilemap.linescroll_enable = 0;
	current_tilemap.linescroll_interval = 0;
	current_tilemap.linescroll_table_address = 0;
	current_tilemap.vertical_linescroll_enable = 0;
	current_tilemap.linezoom_enable = 0;
	current_tilemap.vertical_cell_scroll_enable = 0;

	current_tilemap.line_screen_enabled = VDP2_R0LCEN;
	current_tilemap.mosaic_screen_enabled = VDP2_R0MZE;

	/*Use 0x80 as a normal/rotate switch*/
	current_tilemap.layer_name=0x80;

	if ( !current_tilemap.enabled ) return;

	switch(VDP2_RPMD)
	{
		case 0:/*Rotation Parameter A */
			current_tilemap.roz_mode3 = 0;
			vdp2_draw_rotation_screen(bitmap, cliprect, 1 );
			break;
		case 1:/*Rotation Parameter B */
		/*case 2: */
			current_tilemap.roz_mode3 = 0;
			vdp2_draw_rotation_screen(bitmap, cliprect, 2 );
			break;
		case 2:/*Rotation Parameter A & B CKTE */
			current_tilemap.roz_mode3 = 0;
			vdp2_draw_rotation_screen(bitmap, cliprect, 2 );
			vdp2_draw_rotation_screen(bitmap, cliprect, 1 );
			break;
		case 3:/*Rotation Parameter A & B Window */
			current_tilemap.roz_mode3 = 1;
			vdp2_draw_rotation_screen(bitmap, cliprect, 2 );
			vdp2_draw_rotation_screen(bitmap, cliprect, 1 );
			break;
	}

}

static void vdp2_draw_back(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int y;
	int x;
	UINT8 const *const gfxdata = vdp2l.gfx_decode;

	UINT8 interlace = (get_lsmd() == 3)+1;

/*  popmessage("Back screen %08x %08x %08x",get_bdclmd(),VDP2_BKCLMD,VDP2_BKTA); */

	/* draw black if BDCLMD and DISP are cleared */
	if(!(get_bdclmd()) && !(get_disp()))
		bitmap_fill32(bitmap, cliprect, 0);
	else
	{
		UINT32 base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;

		for(y=cliprect->min_y;y<=cliprect->max_y;y++)
		{
			UINT32 base_offs = ((VDP2_BKTA ) & base_mask) << 1;
			UINT16 dot;
			int r, g, b;
			UINT32 pix, *line = BMP32(bitmap, y);

			/* the back screen is one colour per line */
			if(VDP2_BKCLMD)
				base_offs += ((y / interlace) << 1);
			dot = (gfxdata[base_offs+0]<<8)|gfxdata[base_offs+1];
			b = pal5bit((dot & 0x7c00) >> 10);
			g = pal5bit((dot & 0x03e0) >> 5);
			r = pal5bit( dot & 0x001f);
			if(VDP2_BKCOEN)
				vdp2_compute_color_offset( &r, &g, &b, VDP2_BKCOSL );
			pix = RGB_MAKE(r, g, b);

			for(x=cliprect->min_x;x<=cliprect->max_x;x++)
				line[x] = pix;
		}
	}
}

static UINT32 vdp2_vram_r(offs_t offset)
{
	return vdp2_vram[offset];
}

static void vdp2_vram_w(offs_t offset, UINT32 data, UINT32 mem_mask)
{
	UINT8* gfxdata = vdp2l.gfx_decode;
	const UINT32 old_data = vdp2_vram[offset];

	COMBINE_DATA(&vdp2_vram[offset]);

	/* rewriting the same value leaves the decoded tiles and the RBG0 cache valid */
	if (vdp2_vram[offset] == old_data)
		return;

	data = vdp2_vram[offset];
	/* put in gfx region for easy decoding */
	gfxdata[offset*4+0] = (data & 0xff000000) >> 24;
	gfxdata[offset*4+1] = (data & 0x00ff0000) >> 16;
	gfxdata[offset*4+2] = (data & 0x0000ff00) >> 8;
	gfxdata[offset*4+3] = (data & 0x000000ff) >> 0;

	/* the tiles with these bytes (the 8-bit ones overlap the previous one) */
	gfx_mark_dirty_addr(offset*4);

	if ( RBG0_cache_data.watch_vdp2_vram_writes )
	{
		int i;
		for (i = 0; i < 2; i++)
		{
			if ( !(RBG0_cache_data.watch_vdp2_vram_writes & (1 << i)) )
				continue;

			if ( offset >= RBG0_cache_data.tile_offset_min[i] &&
					offset < RBG0_cache_data.tile_offset_max[i] )
			{
				/* character data changed, rebuild everything */
				log_masked(LOG_VDP2, "RBG Cache: dirtying for RP = %d, write at offset = %06X\n", i + 1, offset);
				RBG0_cache_data.is_cache_dirty |= 1 << i;
				RBG0_cache_data.watch_vdp2_vram_writes &= ~(1 << i);
			}
			else if ( offset >= RBG0_cache_data.map_offset_min[i] &&
					offset < RBG0_cache_data.map_offset_max[i] )
			{
				/* a pattern name changed, only its cells need redrawing */
				if ( RBG0_cache_data.map_dirty_max[i] <= RBG0_cache_data.map_dirty_min[i] )
				{
					RBG0_cache_data.map_dirty_min[i] = offset;
					RBG0_cache_data.map_dirty_max[i] = offset + 1;
				}
				else if ( offset < RBG0_cache_data.map_dirty_min[i] )
					RBG0_cache_data.map_dirty_min[i] = offset;
				else if ( offset >= RBG0_cache_data.map_dirty_max[i] )
					RBG0_cache_data.map_dirty_max[i] = offset + 1;
			}
		}
	}
}

static UINT16 vdp2_regs_r(offs_t offset)
{
	return vdp2_regs[offset];
}

/* TODO: mode 0 handling */
static UINT32 vdp2_cram_r(offs_t offset)
{
	offset &= (0xfff) >> (2);
	return vdp2_cram[offset];
}

/* TODO: byte writes are goofy */
static void vdp2_cram_w(offs_t offset, UINT32 data, UINT32 mem_mask)
{
	int i, j;
	int r,g,b;
	UINT8 cmode0;
	UINT32 old_data;

	cmode0 = (VDP2_CRMD & 3) == 0;

	offset &= (0xfff) >> (2);
	old_data = vdp2_cram[offset];
	COMBINE_DATA(&vdp2_cram[offset]);

	/* the RBG0 cache holds rendered colours, so changing one it used makes it stale */
	/* (colour RAM mode 0 mirrors the palette at 0x400, so check both halves) */
	if (vdp2_cram[offset] != old_data)
	{
		const UINT32 pen = ((VDP2_CRMD & 2) ? offset : (offset * 2)) & 0x3ff;
		const UINT32 count = (VDP2_CRMD & 2) ? 1 : 2;
		for (i = 0; i < 2; i++)
		{
			for (j = 0; j < 2; j++)
			{
				const UINT32 p = j ? (pen | 0x400) : pen;
				if ((p < RBG0_cache_data.pen_max[i]) && ((p + count) > RBG0_cache_data.pen_min[i]))
				{
					RBG0_cache_data.is_cache_dirty |= 1 << i;
				}
			}
		}
	}

	switch( VDP2_CRMD )
	{
		/*Mode 2/3*/
		case 2:
		case 3:
		{
			/*offset &= (0xfff) >> 2; */

			b = ((vdp2_cram[offset] & 0x00ff0000) >> 16);
			g = ((vdp2_cram[offset] & 0x0000ff00) >> 8);
			r = ((vdp2_cram[offset] & 0x000000ff) >> 0);
			set_pen(offset,RGB_MAKE(r,g,b));
			set_pen(offset^0x400,RGB_MAKE(r,g,b));
		}
		break;
		/*Mode 0*/
		case 0:
		case 1:
		{
			offset &= (0xfff) >> (cmode0+2);

			b = ((vdp2_cram[offset] & 0x00007c00) >> 10);
			g = ((vdp2_cram[offset] & 0x000003e0) >> 5);
			r = ((vdp2_cram[offset] & 0x0000001f) >> 0);
			set_pen((offset*2)+1, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
			if(cmode0)
				set_pen(((offset*2)+1)^0x400, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));

			b = ((vdp2_cram[offset] & 0x7c000000) >> 26);
			g = ((vdp2_cram[offset] & 0x03e00000) >> 21);
			r = ((vdp2_cram[offset] & 0x001f0000) >> 16);
			set_pen(offset*2, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
			if(cmode0)
				set_pen((offset*2)^0x400, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
		}
		break;
	}
}

static void refresh_palette_data(void)
{
	int r,g,b;
	int c_i;
	UINT8 bank;

	RBG0_cache_data.is_cache_dirty |= 3;

	switch( VDP2_CRMD )
	{
		case 2:
		case 3:
		{
			for(c_i=0;c_i<0x400;c_i++)
			{
				b = ((vdp2_cram[c_i] & 0x00ff0000) >> 16);
				g = ((vdp2_cram[c_i] & 0x0000ff00) >> 8);
				r = ((vdp2_cram[c_i] & 0x000000ff) >> 0);
				set_pen(c_i,RGB_MAKE(r,g,b));
				set_pen(c_i+0x400,RGB_MAKE(r,g,b));
			}
		}
		break;
		case 0:
		{
			for(bank=0;bank<2;bank++)
			{
				for(c_i=0;c_i<0x400;c_i++)
				{
					b = ((vdp2_cram[c_i] & 0x00007c00) >> 10);
					g = ((vdp2_cram[c_i] & 0x000003e0) >> 5);
					r = ((vdp2_cram[c_i] & 0x0000001f) >> 0);
					set_pen((c_i*2)+1+bank*0x400, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
					b = ((vdp2_cram[c_i] & 0x7c000000) >> 26);
					g = ((vdp2_cram[c_i] & 0x03e00000) >> 21);
					r = ((vdp2_cram[c_i] & 0x001f0000) >> 16);
					set_pen(c_i*2+bank*0x400, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
				}
			}
		}
		break;
		case 1:
		{
			for(c_i=0;c_i<0x800;c_i++)
			{
				b = ((vdp2_cram[c_i] & 0x00007c00) >> 10);
				g = ((vdp2_cram[c_i] & 0x000003e0) >> 5);
				r = ((vdp2_cram[c_i] & 0x0000001f) >> 0);
				set_pen((c_i*2)+1, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
				b = ((vdp2_cram[c_i] & 0x7c000000) >> 26);
				g = ((vdp2_cram[c_i] & 0x03e00000) >> 21);
				r = ((vdp2_cram[c_i] & 0x001f0000) >> 16);
				set_pen(c_i*2, RGB_MAKE(pal5bit(r), pal5bit(g), pal5bit(b)));
			}
		}
		break;
	}
}

static void vdp2_regs_w(offs_t offset, UINT16 data, UINT16 mem_mask)
{
	COMBINE_DATA(&vdp2_regs[offset]);

	if(vdp2l.old_crmd != VDP2_CRMD)
	{
		vdp2l.old_crmd = VDP2_CRMD;
		refresh_palette_data();
	}
}


static void vdp2_state_save_postload(void)
{
	UINT8 *gfxdata = vdp2l.gfx_decode;
	int offset;
	UINT32 data;

	for ( offset = 0; offset < 0x100000/4; offset++ )
	{
		data = vdp2_vram[offset];
		/* put in gfx region for easy decoding */
		gfxdata[offset*4+0] = (data & 0xff000000) >> 24;
		gfxdata[offset*4+1] = (data & 0x00ff0000) >> 16;
		gfxdata[offset*4+2] = (data & 0x0000ff00) >> 8;
		gfxdata[offset*4+3] = (data & 0x000000ff) >> 0;


	}
	for ( offset = 0; offset < 4; offset++ )
		memset(stv_gfx[offset].dirty, 1, stv_gfx[offset].elements);

	memset(&RBG0_cache_data, 0, sizeof(RBG0_cache_data));
	RBG0_cache_data.is_cache_dirty = 3;
	memset(&vdp2_layer_data, 0, sizeof(vdp2_layer_data));

	refresh_palette_data();
}

static int vdp2_start(void)
{
	vdp2_regs = auto_malloc(0x040000);
	vdp2_vram = auto_malloc(0x100000);
	vdp2_cram = auto_malloc(0x080000);
	vdp2l.gfx_decode = auto_malloc(0x100000);
	if (!vdp2_regs || !vdp2_vram || !vdp2_cram || !vdp2l.gfx_decode)
		return 1;
	memset(vdp2_regs, 0, 0x040000);
	memset(vdp2_vram, 0, 0x100000);
	memset(vdp2_cram, 0, 0x080000);
	memset(vdp2l.gfx_decode, 0, 0x100000);
	vdp2l.roz_bitmap[0] = vdp2l.roz_bitmap[1] = NULL;
	vdp2l.old_crmd = -1;

	/* the tiles: 8x8 and 16x16, 4 and 8 bits per pixel (the 8x8 8-bit ones every 32 bytes) */
	vdp2_gfx_source = vdp2l.gfx_decode;
	gfx_setup(&stv_gfx[0],  8,  8, 4,  32, 0x80*3);
	gfx_setup(&stv_gfx[1], 16, 16, 4, 128, 0x80*3);
	gfx_setup(&stv_gfx[2],  8,  8, 8,  32, 0x08*3);
	gfx_setup(&stv_gfx[3], 16, 16, 8, 256, 0x08*3);

	memset(&RBG0_cache_data, 0, sizeof(RBG0_cache_data));
	RBG0_cache_data.is_cache_dirty = 3;
	memset(&vdp2_layer_data, 0, sizeof(vdp2_layer_data));

	state_save_register_UINT16("stv_vdp2", 0, "regs", vdp2_regs, 0x040000/2);
	state_save_register_UINT32("stv_vdp2", 0, "vram", vdp2_vram, 0x100000/4);
	state_save_register_UINT32("stv_vdp2", 0, "cram", vdp2_cram, 0x080000/4);
	state_save_register_func_postload(vdp2_state_save_postload);

	return 0;
}

/*This is for calculating the rgb brightness*/
/*TODO: Optimize this...*/
static void vdp2_fade_effects(void)
{
	/*
	Note:We have to use temporary storages because palette_get_color must use
	variables setted with unsigned int8
	*/
	INT16 t_r,t_g,t_b;
	UINT8 r,g,b;
	rgb_t color;
	int i;
	/*popmessage("%04x %04x",VDP2_CLOFEN,VDP2_CLOFSL); */
	for(i=0;i<2048;i++)
	{
		/*Fade A*/
		color = PEN(i);
		t_r = (VDP2_COAR & 0x100) ? (RGB_RED(color) - (0x100 - (VDP2_COAR & 0xff))) : ((VDP2_COAR & 0xff) + RGB_RED(color));
		t_g = (VDP2_COAG & 0x100) ? (RGB_GREEN(color) - (0x100 - (VDP2_COAG & 0xff))) : ((VDP2_COAG & 0xff) + RGB_GREEN(color));
		t_b = (VDP2_COAB & 0x100) ? (RGB_BLUE(color) - (0x100 - (VDP2_COAB & 0xff))) : ((VDP2_COAB & 0xff) + RGB_BLUE(color));
		if(t_r < 0)     { t_r = 0; }
		if(t_r > 0xff)  { t_r = 0xff; }
		if(t_g < 0)     { t_g = 0; }
		if(t_g > 0xff)  { t_g = 0xff; }
		if(t_b < 0)     { t_b = 0; }
		if(t_b > 0xff)  { t_b = 0xff; }
		r = t_r;
		g = t_g;
		b = t_b;
		set_pen(i+(2048*1),RGB_MAKE(r,g,b));

		/*Fade B*/
		color = PEN(i);
		t_r = (VDP2_COBR & 0x100) ? (RGB_RED(color) - (0xff - (VDP2_COBR & 0xff))) : ((VDP2_COBR & 0xff) + RGB_RED(color));
		t_g = (VDP2_COBG & 0x100) ? (RGB_GREEN(color) - (0xff - (VDP2_COBG & 0xff))) : ((VDP2_COBG & 0xff) + RGB_GREEN(color));
		t_b = (VDP2_COBB & 0x100) ? (RGB_BLUE(color) - (0xff - (VDP2_COBB & 0xff))) : ((VDP2_COBB & 0xff) + RGB_BLUE(color));
		if(t_r < 0)     { t_r = 0; }
		if(t_r > 0xff)  { t_r = 0xff; }
		if(t_g < 0)     { t_g = 0; }
		if(t_g > 0xff)  { t_g = 0xff; }
		if(t_b < 0)     { t_b = 0; }
		if(t_b > 0xff)  { t_b = 0xff; }
		r = t_r;
		g = t_g;
		b = t_b;
		set_pen(i+(2048*2),RGB_MAKE(r,g,b));
	}
	/*popmessage("%04x %04x %04x %04x %04x %04x",VDP2_COAR,VDP2_COAG,VDP2_COAB,VDP2_COBR,VDP2_COBG,VDP2_COBB); */
}

static void vdp2_get_window0_coordinates_calc(int *s_x, int *e_x, int *s_y, int *e_y, int y)
{
	/*W0*/
	switch(get_lsmd())
	{
		case 0:
		case 1:
		case 2:
			*s_y = ((VDP2_W0SY & 0x3ff) >> 0);
			*e_y = ((VDP2_W0EY & 0x3ff) >> 0);
			break;
		case 3:
			*s_y = ((VDP2_W0SY & 0x7ff) >> 0);
			*e_y = ((VDP2_W0EY & 0x7ff) >> 0);
			break;
	}
	/* check if line window is enabled */
	if(VDP2_W0LWE)
	{
		UINT32 base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;
		UINT32 address = (VDP2_W0LWTA & base_mask) * 2;
		/* double density makes the line window to fetch data every two lines */
		UINT8 interlace = (get_lsmd() == 3);
		UINT32 vram_data = vdp2_vram[(address >> 2)+(y >> interlace)];

		*s_x = (vram_data >> 16) & 0x3ff;
		*e_x = (vram_data & 0x3ff);
	}
	else
	{
		switch(get_hreso() & 6)
		{
			/*Normal*/
			case 0:
				*s_x = ((VDP2_W0SX & 0x3fe) >> 1);
				*e_x = ((VDP2_W0EX & 0x3fe) >> 1);
				break;
			/*Hi-Res*/
			case 2:
				*s_x = ((VDP2_W0SX & 0x3ff) >> 0);
				*e_x = ((VDP2_W0EX & 0x3ff) >> 0);
				break;
			/*Exclusive Normal*/
			case 4:
				*s_x = ((VDP2_W0SX & 0x1ff) >> 0);
				*e_x = ((VDP2_W0EX & 0x1ff) >> 0);
				*s_y = ((VDP2_W0SY & 0x3ff) >> 0);
				*e_y = ((VDP2_W0EY & 0x3ff) >> 0);
				break;
			/*Exclusive Hi-Res*/
			case 6:
				*s_x = ((VDP2_W0SX & 0x1ff) << 1);
				*e_x = ((VDP2_W0EX & 0x1ff) << 1);
				*s_y = ((VDP2_W0SY & 0x3ff) >> 0);
				*e_y = ((VDP2_W0EY & 0x3ff) >> 0);
				break;
		}
	}
}

/* the window of a line doesn't change during a screen update, but the rotation
   screens ask for it on every pixel and the tiles on each of their lines: one
   entry per line. Not thread local: with the threaded video a line is only
   drawn by the thread of its strip, and the generation changes before the
   strips start */
static INLINE void vdp2_get_window0_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y)
{
	struct vdp2_window_coord *c;
	if (y < 0 || y >= VDP2_WINDOW_LINES)
	{
		vdp2_get_window0_coordinates_calc(s_x, e_x, s_y, e_y, y);
		return;
	}
	c = &vdp2_window_coords[0][y];
	if (c->gen != vdp2_window_cache_gen)
	{
		vdp2_get_window0_coordinates_calc(&c->sx, &c->ex, &c->sy, &c->ey, y);
		c->gen = vdp2_window_cache_gen;
	}
	*s_x = c->sx; *e_x = c->ex; *s_y = c->sy; *e_y = c->ey;
}

static void vdp2_get_window1_coordinates_calc(int *s_x, int *e_x, int *s_y, int *e_y, int y)
{
	/*W1*/
	switch(get_lsmd())
	{
		case 0:
		case 1:
		case 2:
			*s_y = ((VDP2_W1SY & 0x3ff) >> 0);
			*e_y = ((VDP2_W1EY & 0x3ff) >> 0);
			break;
		case 3:
			*s_y = ((VDP2_W1SY & 0x7ff) >> 0);
			*e_y = ((VDP2_W1EY & 0x7ff) >> 0);
			break;
	}
	/* check if line window is enabled */
	if(VDP2_W1LWE)
	{
		UINT32 base_mask = get_vramsz() ? 0x7ffff : 0x3ffff;
		UINT32 address = (VDP2_W1LWTA & base_mask) * 2;
		/* double density makes the line window to fetch data every two lines */
		UINT8 interlace = (get_lsmd() == 3);
		UINT32 vram_data = vdp2_vram[(address >> 2)+(y >> interlace)];

		*s_x = (vram_data >> 16) & 0x3ff;
		*e_x = (vram_data & 0x3ff);
	}
	else
	{
		switch(get_hreso() & 6)
		{
			/*Normal*/
			case 0:
				*s_x = ((VDP2_W1SX & 0x3fe) >> 1);
				*e_x = ((VDP2_W1EX & 0x3fe) >> 1);
				break;
			/*Hi-Res*/
			case 2:
				*s_x = ((VDP2_W1SX & 0x3ff) >> 0);
				*e_x = ((VDP2_W1EX & 0x3ff) >> 0);
				break;
			/*Exclusive Normal*/
			case 4:
				*s_x = ((VDP2_W1SX & 0x1ff) >> 0);
				*e_x = ((VDP2_W1EX & 0x1ff) >> 0);
				*s_y = ((VDP2_W1SY & 0x3ff) >> 0);
				*e_y = ((VDP2_W1EY & 0x3ff) >> 0);
				break;
			/*Exclusive Hi-Res*/
			case 6:
				*s_x = ((VDP2_W1SX & 0x1ff) << 1);
				*e_x = ((VDP2_W1EX & 0x1ff) << 1);
				*s_y = ((VDP2_W1SY & 0x3ff) >> 0);
				*e_y = ((VDP2_W1EY & 0x3ff) >> 0);
				break;
		}
	}
}

/* the window of a line doesn't change during a screen update, but the rotation
   screens ask for it on every pixel and the tiles on each of their lines: one
   entry per line. Not thread local: with the threaded video a line is only
   drawn by the thread of its strip, and the generation changes before the
   strips start */
static INLINE void vdp2_get_window1_coordinates(int *s_x, int *e_x, int *s_y, int *e_y, int y)
{
	struct vdp2_window_coord *c;
	if (y < 0 || y >= VDP2_WINDOW_LINES)
	{
		vdp2_get_window1_coordinates_calc(s_x, e_x, s_y, e_y, y);
		return;
	}
	c = &vdp2_window_coords[1][y];
	if (c->gen != vdp2_window_cache_gen)
	{
		vdp2_get_window1_coordinates_calc(&c->sx, &c->ex, &c->sy, &c->ey, y);
		c->gen = vdp2_window_cache_gen;
	}
	*s_x = c->sx; *e_x = c->ex; *s_y = c->sy; *e_y = c->ey;
}


static void vdp2_window_line(int y, int key)
{
	int s_x = 0, e_x = 0, s_y = 0, e_y = 0;

	vdp2_wline.gen = vdp2_window_cache_gen;
	vdp2_wline.y = y;
	vdp2_wline.key = key;
	if (current_tilemap.window_control.enabled[0])
	{
		vdp2_get_window0_coordinates(&s_x, &e_x, &s_y, &e_y, y);
		vdp2_wline.s0 = s_x; vdp2_wline.e0 = e_x;
		vdp2_wline.in0 = (y >= s_y && y <= e_y);
	}
	if (current_tilemap.window_control.enabled[1])
	{
		vdp2_get_window1_coordinates(&s_x, &e_x, &s_y, &e_y, y);
		vdp2_wline.s1 = s_x; vdp2_wline.e1 = e_x;
		vdp2_wline.in1 = (y >= s_y && y <= e_y);
	}
}

/* same result as get_window_pixel() on both windows, a disabled window counts as 1 */
static INLINE int vdp2_window_process(int x,int y)
{
	int en0 = current_tilemap.window_control.enabled[0];
	int en1 = current_tilemap.window_control.enabled[1];
	int key, w0_pix = 1, w1_pix = 1;

	if (!en0 && !en1)
		return 1;

	key = (en0 ? 1 : 0) | (en1 ? 2 : 0) | (current_tilemap.window_control.area[0] ? 4 : 0) | (current_tilemap.window_control.area[1] ? 8 : 0);
	if (vdp2_wline.gen != vdp2_window_cache_gen || vdp2_wline.y != y || vdp2_wline.key != key)
		vdp2_window_line(y, key);

	if (en0)
	{
		w0_pix = vdp2_wline.in0 && x >= vdp2_wline.s0 && x <= vdp2_wline.e0;
		if (!current_tilemap.window_control.area[0])
			w0_pix ^= 1;
	}
	if (en1)
	{
		w1_pix = vdp2_wline.in1 && x >= vdp2_wline.s1 && x <= vdp2_wline.e1;
		if (!current_tilemap.window_control.area[1])
			w1_pix ^= 1;
	}

	return current_tilemap.window_control.logic & 1 ? (w0_pix | w1_pix) : (w0_pix & w1_pix);
}

/* TODO: remove this crap. */
static int vdp2_apply_window_on_layer(struct rectangle *cliprect)
{
	int s_x=0,e_x=0,s_y=0,e_y=0;

	if ( current_tilemap.window_control.enabled[0] && (!current_tilemap.window_control.area[0]))
	{
		/* w0, transparent outside supported */
		vdp2_get_window0_coordinates(&s_x, &e_x, &s_y, &e_y, 0);

		if ( s_x > cliprect->min_x ) cliprect->min_x = s_x;
		if ( e_x < cliprect->max_x ) cliprect->max_x = e_x;
		if ( s_y > cliprect->min_y ) cliprect->min_y = s_y;
		if ( e_y < cliprect->max_y ) cliprect->max_y = e_y;

		return 1;
	}
	else if (  current_tilemap.window_control.enabled[1] && (!current_tilemap.window_control.area[1]) )
	{
		/* w1, transparent outside supported */
		vdp2_get_window1_coordinates(&s_x, &e_x, &s_y, &e_y, 0);

		if ( s_x > cliprect->min_x ) cliprect->min_x = s_x;
		if ( e_x < cliprect->max_x ) cliprect->max_x = e_x;
		if ( s_y > cliprect->min_y ) cliprect->min_y = s_y;
		if ( e_y < cliprect->max_y ) cliprect->max_y = e_y;

		return 1;
	}
	else
	{
		return 0;
	}
}

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, UINT8 pri)
{
	int x,y,r,g,b;
	int i;
	UINT16 pix;
	UINT16 *framebuffer_line;
	UINT32 *bitmap_line, *bitmap_line2 = NULL;
	UINT8  interlace_framebuffer;
	UINT8  double_x;
	static const UINT16 sprite_colormask_table[] = {
		0x07ff, 0x07ff, 0x07ff, 0x07ff, 0x03ff, 0x07ff, 0x03ff, 0x01ff,
		0x007f, 0x003f, 0x003f, 0x003f, 0x00ff, 0x00ff, 0x00ff, 0x00ff
	};
	static const UINT16 priority_shift_table[] = { 14, 13, 14, 13, 13, 12, 12, 12, 7, 7, 6, 0, 7, 7, 6, 0 };
	static const UINT16 priority_mask_table[]  = {  3,  7,  1,  3,  3,  7,  7,  7, 1, 1, 3, 0, 1, 1, 3, 0 };
	static const UINT16 ccrr_shift_table[] =     { 11, 11, 11, 11, 10, 11, 10,  9, 0, 6, 0, 6, 0, 6, 0, 6 };
	static const UINT16 ccrr_mask_table[] =      {  7,  3,  7,  3,  7,  1,  3,  7, 0, 1, 0, 3, 0, 1, 0, 3 };
	static const UINT16 shadow_mask_table[] = { 0, 0, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0, 0, 0, 0, 0, 0, 0, 0 };
	UINT16 alpha_enabled;

	int sprite_type;
	int sprite_colormask;
	int color_offset_pal;
	int sprite_shadow;
	UINT16 sprite_priority_shift, sprite_priority_mask, sprite_ccrr_shift, sprite_ccrr_mask;
	UINT8   priority;
	UINT8   ccr = 0;
	UINT8 sprite_priorities[8];
	UINT8 sprite_ccr[8];
	int sprite_color_mode = VDP2_SPCLMD;
	int sprite_window;

	if ( (vdp1_sprite_priorities_usage_valid == 1) && (vdp1_sprite_priorities_used[pri] == 0) )
		return;

	sprite_priorities[0] = VDP2_S0PRIN;
	sprite_priorities[1] = VDP2_S1PRIN;
	sprite_priorities[2] = VDP2_S2PRIN;
	sprite_priorities[3] = VDP2_S3PRIN;
	sprite_priorities[4] = VDP2_S4PRIN;
	sprite_priorities[5] = VDP2_S5PRIN;
	sprite_priorities[6] = VDP2_S6PRIN;
	sprite_priorities[7] = VDP2_S7PRIN;

	sprite_ccr[0] = VDP2_S0CCRT;
	sprite_ccr[1] = VDP2_S1CCRT;
	sprite_ccr[2] = VDP2_S2CCRT;
	sprite_ccr[3] = VDP2_S3CCRT;
	sprite_ccr[4] = VDP2_S4CCRT;
	sprite_ccr[5] = VDP2_S5CCRT;
	sprite_ccr[6] = VDP2_S6CCRT;
	sprite_ccr[7] = VDP2_S7CCRT;

	sprite_type = VDP2_SPTYPE;
	sprite_colormask = sprite_colormask_table[sprite_type];
	sprite_priority_shift = priority_shift_table[sprite_type];
	sprite_priority_mask = priority_mask_table[sprite_type];
	sprite_ccrr_shift = ccrr_shift_table[sprite_type];
	sprite_ccrr_mask = ccrr_mask_table[sprite_type];
	sprite_shadow = shadow_mask_table[sprite_type];

	for ( i = 0; i < (sprite_priority_mask+1); i++ ) if ( sprite_priorities[i] == pri ) break;
	if ( i == (sprite_priority_mask+1) ) return;

	/* color offset (RGB brightness) */
	color_offset_pal = 0;
	if ( VDP2_SPCOEN )
	{
		if ( VDP2_SPCOSL == 0 )
		{ color_offset_pal = 2048; }
		else
		{ color_offset_pal = 2048*2; }
	}

	/* color calculation (alpha blending)*/
	if ( VDP2_SPCCEN )
	{
		alpha_enabled = 0;
		switch( VDP2_SPCCCS )
		{
			case 0x0: if ( pri <= VDP2_SPCCN ) alpha_enabled = 1; break;
			case 0x1: if ( pri == VDP2_SPCCN ) alpha_enabled = 1; break;
			case 0x2: if ( pri >= VDP2_SPCCN ) alpha_enabled = 1; break;
			case 0x3: alpha_enabled = 2; sprite_shadow = 0; break;
		}
	}
	else
	{
		alpha_enabled = 0;
	}

	/* framebuffer interlace */
	if ( (get_lsmd() == 3) && vdp1.framebuffer_double_interlace == 0 )
		interlace_framebuffer = 1;
	else
		interlace_framebuffer = 0;

	/*Guess:Some games needs that the horizontal sprite size to be doubled
	  (TODO: understand the proper settings,it might not work like this)*/
	if(VDP1_TVM() == 0 && BIT(get_hreso(), 1)) /* astrass & findlove */
		double_x = 1;
	else
		double_x = 0;

	/* window control */
	current_tilemap.window_control.logic = VDP2_SPLOG;
	current_tilemap.window_control.enabled[0] = VDP2_SPW0E;
	current_tilemap.window_control.enabled[1] = VDP2_SPW1E;
/*  current_tilemap.window_control.? = VDP2_SPSWE; */
	current_tilemap.window_control.area[0] = VDP2_SPW0A;
	current_tilemap.window_control.area[1] = VDP2_SPW1A;
/*  current_tilemap.window_control.? = VDP2_SPSWA; */

	/* several games attempt to use sprite window with an illegal type 1 */
	/* even if document explicitly states they won't work. */
	/* (reportedly seen with a SPCTL of 0x30f1, and bits 7 & 6 are <undefined> there). */
	/* - kingbox (gameplay, sets 0x3031) */
	/* - raymanj (corrupted tiles when showing stage intro) */
	/* - sandor (player feet during attract intro) */
	/* - samsho4 (character select & gameplay) */
	/* TODO: document also states that color mode must be zero */
	/* - but pukunpa (already) uses mode 1 and wants this enabled, mistake? */
	sprite_window = VDP2_SPWINEN && sprite_type >= 2 && sprite_type <= 7;

/*  vdp2_apply_window_on_layer(mycliprect); */

	/*if (VDP2_SPWINEN) */
	/*	popmessage("(%d %d) enable mask %d type %d | color %d alpha %d shadow %d", interlace_framebuffer, double_x,	sprite_window, sprite_type, sprite_color_mode, alpha_enabled, sprite_shadow); */

	/* TODO: reminder that this is an unfollowable snippet ... */
	if (interlace_framebuffer == 0 && double_x == 0 )
	{
		if ( alpha_enabled == 0 )
		{
			for ( y = cliprect->min_y; y <= cliprect->max_y; y++ )
			{
				int x0 = cliprect->min_x, x1 = cliprect->max_x;
				if ( vdp1_sprite_priorities_usage_valid )
				{
					if (vdp1_sprite_priorities_in_fb_line[y][pri] == 0)
						continue;
					/* the other pixels of the line only belong to other passes */
					if (x0 < vdp1_sprite_priorities_min_x[y][pri]) x0 = vdp1_sprite_priorities_min_x[y][pri];
					if (x1 > vdp1_sprite_priorities_max_x[y][pri]) x1 = vdp1_sprite_priorities_max_x[y][pri];
				}

				framebuffer_line = vdp1.framebuffer_display_lines[y];
				bitmap_line = BMP32(bitmap, y);

				for ( x = x0; x <= x1; x++ )
				{
					pix = framebuffer_line[x];
					/* a zero pixel draws nothing in any pass: it does not mark its priority either */
					if (pix == 0)
					{
						/* in a run of zero pixels, the next four at once */
						while (x + 4 <= x1)
						{
							UINT64 next4;
							memcpy(&next4, framebuffer_line + x + 1, 8);
							if (next4 != 0)
								break;
							x += 4;
						}
						continue;
					}
					if(!vdp2_window_process(x,y))
						continue;

					/* pukunpa, no alpha no framebuffer bumps */
					if(sprite_window && pix == 0x8000)
						continue;

					if ( (pix & 0x8000) && sprite_color_mode)
					{
						if ( sprite_priorities[0] != pri )
						{
							SPRITE_PRIORITY_SEEN(sprite_priorities[0], x, y);
							continue;
						};

						b = pal5bit((pix & 0x7c00) >> 10);
						g = pal5bit((pix & 0x03e0) >> 5);
						r = pal5bit( pix & 0x001f);
						if ( color_offset_pal )
						{
							vdp2_compute_color_offset( &r, &g, &b, VDP2_SPCOSL );
						}

						bitmap_line[x] = RGB_MAKE(r, g, b);
					}
					else
					{
						priority = sprite_priorities[(pix >> sprite_priority_shift) & sprite_priority_mask];
						if ( priority != pri )
						{
							SPRITE_PRIORITY_SEEN(priority, x, y);
							continue;
						};

						/* Pretty Fighter X, Game Tengoku shadows */
						/* TODO: Pretty Fighter X doesn't read what's behind on title screen, VDP1 bug? */
						/* TODO: seldomly Game Tengoku shadows aren't drawn properly */
						/* TODO: allegedly can't enable this with sprite window (verify) */
						if(pix & 0x8000 && VDP2_SDCTL & 0x100 && !sprite_window)
						{
							rgb_t p = bitmap_line[x];
							bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
						}
						else
						{
							pix &= sprite_colormask;
							if ( pix == (sprite_colormask - 1) )
							{
								/*shadow - in reality, we should check from what layer pixel beneath comes...*/
								if ( VDP2_SDCTL & 0x3f )
								{
									rgb_t p = bitmap_line[x];
									bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
								}
								/* note that when shadows are disabled, "shadow" palette entries are not drawn */
							}
							else if ( pix )
							{
								pix += (VDP2_SPCAOS << 8);
								pix &= 0x7ff;
								pix += color_offset_pal;
								bitmap_line[x] = PEN( pix );

							}
						}

						/* TODO: I don't think this one makes much sense ... (1) */
						if ( pix & sprite_shadow )
						{
							if ( pix & ~sprite_shadow )
							{
								rgb_t p = bitmap_line[x];
								bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
							}
						}
					}
				}
			}
		}
		else /*alpha_enabled == 1 */
		{
			for ( y = cliprect->min_y; y <= cliprect->max_y; y++ )
			{
				int x0 = cliprect->min_x, x1 = cliprect->max_x;
				if ( vdp1_sprite_priorities_usage_valid )
				{
					if (vdp1_sprite_priorities_in_fb_line[y][pri] == 0)
						continue;
					/* the other pixels of the line only belong to other passes */
					if (x0 < vdp1_sprite_priorities_min_x[y][pri]) x0 = vdp1_sprite_priorities_min_x[y][pri];
					if (x1 > vdp1_sprite_priorities_max_x[y][pri]) x1 = vdp1_sprite_priorities_max_x[y][pri];
				}

				framebuffer_line = vdp1.framebuffer_display_lines[y];
				bitmap_line = BMP32(bitmap, y);

				for ( x = x0; x <= x1; x++ )
				{
					pix = framebuffer_line[x];
					/* a zero pixel draws nothing in any pass: it does not mark its priority either */
					if (pix == 0)
					{
						/* in a run of zero pixels, the next four at once */
						while (x + 4 <= x1)
						{
							UINT64 next4;
							memcpy(&next4, framebuffer_line + x + 1, 8);
							if (next4 != 0)
								break;
							x += 4;
						}
						continue;
					}
					if(!vdp2_window_process(x,y))
						continue;

					/* raymanj on FMV, alpha enabled (no noticeable difference?) */
					if(sprite_window && pix == 0x8000)
						continue;

					if ( (pix & 0x8000) && sprite_color_mode)
					{
						if ( sprite_priorities[0] != pri )
						{
							SPRITE_PRIORITY_SEEN(sprite_priorities[0], x, y);
							continue;
						};


						b = pal5bit((pix & 0x7c00) >> 10);
						g = pal5bit((pix & 0x03e0) >> 5);
						r = pal5bit( pix & 0x001f);
						if ( color_offset_pal )
						{
							vdp2_compute_color_offset( &r, &g, &b, VDP2_SPCOSL );
						}
						ccr = sprite_ccr[0];
						if ( VDP2_CCMD )
						{
							bitmap_line[x] = add_blend_r32( bitmap_line[x], RGB_MAKE(r, g, b));
						}
						else
						{
							bitmap_line[x] = stv_alpha_blend_r32( bitmap_line[x], RGB_MAKE(r, g ,b), ((UINT16)(0x1f-ccr)*0xff)/0x1f);
						}
					}
					else
					{
						priority = sprite_priorities[(pix >> sprite_priority_shift) & sprite_priority_mask];
						if ( priority != pri )
						{
							SPRITE_PRIORITY_SEEN(priority, x, y);
							continue;
						};

						ccr = sprite_ccr[ (pix >> sprite_ccrr_shift) & sprite_ccrr_mask ];
						if ( alpha_enabled == 2 )
						{
							if ( ( pix & 0x8000 ) == 0 )
							{
								ccr = 0;
							}
						}


						{
							pix &= sprite_colormask;
							if ( pix == (sprite_colormask - 1) )
							{
								/*shadow - in reality, we should check from what layer pixel beneath comes...*/
								if ( VDP2_SDCTL & 0x3f )
								{
									rgb_t p = bitmap_line[x];
									bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
								}
								/* note that when shadows are disabled, "shadow" palette entries are not drawn */
							} else if ( pix )
							{
								pix += (VDP2_SPCAOS << 8);
								pix &= 0x7ff;
								pix += color_offset_pal;
								if ( ccr > 0 )
								{
									if ( VDP2_CCMD )
									{
										bitmap_line[x] = add_blend_r32( bitmap_line[x], PEN(pix) );
									}
									else
									{
										bitmap_line[x] = stv_alpha_blend_r32( bitmap_line[x], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
									}
								}
								else
									bitmap_line[x] = PEN(pix);
							}
						}

						/* TODO: (1) */
						if ( pix & sprite_shadow )
						{
							if ( pix & ~sprite_shadow )
							{
								rgb_t p = bitmap_line[x];
								bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		/* framebuffer line y is drawn on screen lines 2y and 2y + 1 when interlaced */
		for ( y = cliprect->min_y / (interlace_framebuffer+1); y <= cliprect->max_y / (interlace_framebuffer+1); y++ )
		{
			if ( vdp1_sprite_priorities_usage_valid )
				if (vdp1_sprite_priorities_in_fb_line[y][pri] == 0)
					continue;

			framebuffer_line = vdp1.framebuffer_display_lines[y];
			if ( interlace_framebuffer == 0 )
			{
				bitmap_line = BMP32(bitmap, y);
			}
			else
			{
				bitmap_line = BMP32(bitmap, 2*y);
				bitmap_line2 = BMP32(bitmap, 2*y + 1);
			}

			for ( x = cliprect->min_x; x <= cliprect->max_x /(double_x+1) ; x++ )
			{
				pix = framebuffer_line[x];
				/* a zero pixel draws nothing in any pass: it does not mark its priority either */
				if (pix == 0)
					continue;
				if(!vdp2_window_process(x,y))
					continue;

				/* amoudan, interlaced case */
				if(sprite_window && pix == 0x8000)
					continue;

				if ( (pix & 0x8000) && sprite_color_mode)
				{
					if ( sprite_priorities[0] != pri )
					{
						SPRITE_PRIORITY_SEEN(sprite_priorities[0], x, y);
						continue;
					};

					b = pal5bit((pix & 0x7c00) >> 10);
					g = pal5bit((pix & 0x03e0) >> 5);
					r = pal5bit( pix & 0x001f);
					if ( color_offset_pal )
					{
						vdp2_compute_color_offset( &r, &g, &b, VDP2_SPCOSL );
					}
					if ( alpha_enabled == 0 )
					{
						if(double_x)
						{
							bitmap_line[x*2] = RGB_MAKE(r, g, b);
							if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = RGB_MAKE(r, g, b);
							bitmap_line[x*2+1] = RGB_MAKE(r, g, b);
							if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = RGB_MAKE(r, g, b);
						}
						else
						{
							bitmap_line[x] = RGB_MAKE(r, g, b);
							if ( interlace_framebuffer == 1 ) bitmap_line2[x] = RGB_MAKE(r, g, b);
						}
					}
					else /* alpha_blend == 1 */
					{
						ccr = sprite_ccr[0];

						if ( VDP2_CCMD )
						{
							if(double_x)
							{
								bitmap_line[x*2] = add_blend_r32( bitmap_line[x*2], RGB_MAKE(r, g, b) );
								if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = add_blend_r32( bitmap_line2[x*2], RGB_MAKE(r, g, b) );
								bitmap_line[x*2+1] = add_blend_r32( bitmap_line[x*2+1], RGB_MAKE(r, g, b) );
								if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = add_blend_r32( bitmap_line2[x*2+1], RGB_MAKE(r, g, b) );
							}
							else
							{
								bitmap_line[x] = add_blend_r32( bitmap_line[x], RGB_MAKE(r, g, b) );
								if ( interlace_framebuffer == 1 ) bitmap_line2[x] = add_blend_r32( bitmap_line2[x], RGB_MAKE(r, g, b) );
							}
						}
						else
						{
							if(double_x)
							{
								bitmap_line[x*2] = stv_alpha_blend_r32( bitmap_line[x*2], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
								if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = stv_alpha_blend_r32( bitmap_line2[x*2], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
								bitmap_line[x*2+1] = stv_alpha_blend_r32( bitmap_line[x*2+1], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
								if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = stv_alpha_blend_r32( bitmap_line2[x*2+1], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f);
							}
							else
							{
								bitmap_line[x] = stv_alpha_blend_r32( bitmap_line[x], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f);
								if ( interlace_framebuffer == 1 ) bitmap_line2[x] = stv_alpha_blend_r32( bitmap_line2[x], RGB_MAKE(r, g, b), ((UINT16)(0x1f-ccr)*0xff)/0x1f);
							}
						}
					}
				}
				else
				{
					priority = sprite_priorities[(pix >> sprite_priority_shift) & sprite_priority_mask];
					if ( priority != pri )
					{
						SPRITE_PRIORITY_SEEN(priority, x, y);
						continue;
					};

					if ( alpha_enabled )
						ccr = sprite_ccr[ (pix >> sprite_ccrr_shift) & sprite_ccrr_mask ];

					if ( alpha_enabled == 2 )
					{
						if ( ( pix & 0x8000 ) == 0 )
						{
							ccr = 0;
						}
					}

					{
						pix &= sprite_colormask;
						if ( pix == (sprite_colormask - 1) )
						{
							/*shadow - in reality, we should check from what layer pixel beneath comes...*/
							if ( VDP2_SDCTL & 0x3f )
							{
								rgb_t p = bitmap_line[x];
								if(double_x)
								{
									p = bitmap_line[x*2];
									bitmap_line[x*2] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
									p = bitmap_line[x*2+1];
									bitmap_line[x*2+1] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
								}
								else
									bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
							}
							/* note that when shadows are disabled, "shadow" palette entries are not drawn */
						} else if ( pix )
						{
							pix += (VDP2_SPCAOS << 8);
							pix &= 0x7ff;
							pix += color_offset_pal;
							if ( alpha_enabled == 0 )
							{
								if(double_x)
								{
									bitmap_line[x*2] = PEN( pix );
									if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = PEN( pix );
									bitmap_line[x*2+1] = PEN( pix );
									if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = PEN( pix );
								}
								else
								{
									bitmap_line[x] = PEN( pix );
									if ( interlace_framebuffer == 1 ) bitmap_line2[x] = PEN( pix );
								}
							}
							else /* alpha_blend == 1 */
							{
								if ( VDP2_CCMD )
								{
									if(double_x)
									{
										bitmap_line[x*2] = add_blend_r32( bitmap_line[x*2], PEN(pix) );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = add_blend_r32( bitmap_line2[x], PEN(pix) );
										bitmap_line[x*2+1] = add_blend_r32( bitmap_line[x*2+1], PEN(pix) );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = add_blend_r32( bitmap_line2[x], PEN(pix) );
									}
									else
									{
										bitmap_line[x] = add_blend_r32( bitmap_line[x], PEN(pix) );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x] = add_blend_r32( bitmap_line2[x], PEN(pix) );
									}
								}
								else
								{
									if(double_x)
									{
										bitmap_line[x*2] = stv_alpha_blend_r32( bitmap_line[x*2], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x*2] = stv_alpha_blend_r32( bitmap_line2[x], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
										bitmap_line[x*2+1] = stv_alpha_blend_r32( bitmap_line[x*2+1], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x*2+1] = stv_alpha_blend_r32( bitmap_line2[x], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
									}
									else
									{
										bitmap_line[x] = stv_alpha_blend_r32( bitmap_line[x], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
										if ( interlace_framebuffer == 1 ) bitmap_line2[x] = stv_alpha_blend_r32( bitmap_line2[x], PEN(pix), ((UINT16)(0x1f-ccr)*0xff)/0x1f );
									}
								}
							}
						}
					}

					/* TODO: (1) */
					if ( pix & sprite_shadow )
					{
						if ( pix & ~sprite_shadow )
						{
							rgb_t p = bitmap_line[x];
							if(double_x)
							{
								p = bitmap_line[x*2];
								bitmap_line[x*2] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
								p = bitmap_line[x*2+1];
								bitmap_line[x*2+1] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
							}
							else
								bitmap_line[x] = RGB_MAKE(RGB_RED(p) >> 1, RGB_GREEN(p) >> 1, RGB_BLUE(p) >> 1);
						}
					}
				}
			}
		}
	}

	vdp1_sprite_priorities_usage_valid = 1;
}

/* the layers and sprites by priority (with the sprites when 'sprites' is set) */
static void vdp2_render_layers(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int sprites)
{
	UINT8 pri;

	vdp1_sprite_priorities_usage_valid = 0;
	memset(vdp1_sprite_priorities_used, 0, sizeof(vdp1_sprite_priorities_used));

	/*If a plane has a priority value of zero it isn't shown at all.*/
	for(pri=1;pri<8;pri++)
	{
		if(pri==VDP2_N3PRIN) { vdp2_draw_NBG3(bitmap,cliprect); }
		if(pri==VDP2_N2PRIN) { vdp2_draw_NBG2(bitmap,cliprect); }
		if(pri==VDP2_N1PRIN) { vdp2_draw_NBG1(bitmap,cliprect); }
		if(pri==VDP2_N0PRIN) { vdp2_draw_NBG0(bitmap,cliprect); }
		if(pri==VDP2_R0PRIN) { vdp2_draw_RBG0(bitmap,cliprect); }
		if (sprites) { draw_sprites(bitmap,cliprect,pri); }
	}
}

#ifdef STV_VIDEO_THREADS
#include <rthreads/rthreads.h>

#define VDP2_MAX_THREADS	4

/* the band workers: band 0 is the main thread's, worker i draws band i */
static struct
{
	sthread_t *thread[VDP2_MAX_THREADS];
	int index[VDP2_MAX_THREADS];
	slock_t *lock;
	scond_t *start, *done;
	int workers;		/* threads started */
	int gen;			/* job number */
	int bands;			/* bands of the job */
	int pending;		/* worker bands not drawn yet */
	int quit;
	struct mame_bitmap *bitmap;
	struct rectangle band[VDP2_MAX_THREADS];
} vdp2_mt;

/*
	Thread i draws the strips i, i + n, i + 2n... of VDP2_STRIP lines:
	the layers are not spread evenly over the screen (a field at the
	bottom, a sky at the top), strips keep the threads equally busy.
*/
#define VDP2_STRIP	16

static void vdp2_render_band(struct mame_bitmap *bitmap, const struct rectangle *screen, int i, int n)
{
	int y;
	vdp2_band_index = i;
	vdp2_band_decoded_count[i] = 0;
	for (y = screen->min_y + i * VDP2_STRIP; y <= screen->max_y; y += n * VDP2_STRIP)
	{
		struct rectangle strip = *screen;
		strip.min_y = y;
		strip.max_y = y + VDP2_STRIP - 1;
		if (strip.max_y > screen->max_y)
			strip.max_y = screen->max_y;
		vdp2_render_layers(bitmap, &strip, 1);
	}
	vdp2_band_index = -1;
}

static void vdp2_band_worker(void *arg)
{
	const int i = *(const int *)arg;
	int seen = 0;

	slock_lock(vdp2_mt.lock);
	for (;;)
	{
		while (vdp2_mt.gen == seen && !vdp2_mt.quit)
			scond_wait(vdp2_mt.start, vdp2_mt.lock);
		if (vdp2_mt.quit)
			break;
		seen = vdp2_mt.gen;
		if (i < vdp2_mt.bands)
		{
			struct rectangle screen = vdp2_mt.band[0];
			struct mame_bitmap *bitmap = vdp2_mt.bitmap;
			int bands = vdp2_mt.bands;
			slock_unlock(vdp2_mt.lock);
			vdp2_render_band(bitmap, &screen, i, bands);
			slock_lock(vdp2_mt.lock);
			if (--vdp2_mt.pending == 0)
				scond_signal(vdp2_mt.done);
		}
	}
	slock_unlock(vdp2_mt.lock);
}

static void vdp2_threads_stop(void)
{
	int i;
	if (!vdp2_mt.lock)
		return;
	slock_lock(vdp2_mt.lock);
	vdp2_mt.quit = 1;
	scond_broadcast(vdp2_mt.start);
	slock_unlock(vdp2_mt.lock);
	for (i = 1; i < vdp2_mt.workers + 1; i++)
		sthread_join(vdp2_mt.thread[i]);
	scond_free(vdp2_mt.start);
	scond_free(vdp2_mt.done);
	slock_free(vdp2_mt.lock);
	memset(&vdp2_mt, 0, sizeof(vdp2_mt));
	for (i = 0; i < VDP2_MAX_THREADS; i++)
	{
		free(vdp2_band_decoded[i]);
		vdp2_band_decoded[i] = NULL;
		vdp2_band_decoded_count[i] = 0;
	}
}

/* the screen in bands, 0 if the threads are not there (then the caller draws it alone) */
static int vdp2_render_threaded(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int bands = options.stv_video_threads, i;

	if (bands > VDP2_MAX_THREADS)
		bands = VDP2_MAX_THREADS;
	if (!vdp2_mt.lock)
	{
		vdp2_mt.lock = slock_new();
		vdp2_mt.start = scond_new();
		vdp2_mt.done = scond_new();
		if (!vdp2_mt.lock || !vdp2_mt.start || !vdp2_mt.done)
			return 0;
	}
	while (vdp2_mt.workers < bands - 1)
	{
		i = vdp2_mt.workers + 1;
		vdp2_mt.index[i] = i;
		vdp2_mt.thread[i] = sthread_create(vdp2_band_worker, &vdp2_mt.index[i]);
		if (!vdp2_mt.thread[i])
			break;
		vdp2_mt.workers++;
	}
	if (bands > vdp2_mt.workers + 1)
		bands = vdp2_mt.workers + 1;
	if (bands < 2)
		return 0;
	for (i = 0; i < bands; i++)
		if (!vdp2_band_decoded[i])
		{
			vdp2_band_decoded[i] = malloc(VDP2_DECODED_MAX * sizeof(UINT32));
			if (!vdp2_band_decoded[i])
				return 0;
		}

	/* the rotation plane caches first, alone */
	vdp2_render_mode = VDP2_RENDER_PREPASS;
	vdp2_render_layers(bitmap, cliprect, 0);

	/* then the bands */
	vdp2_render_mode = VDP2_RENDER_BAND;
	vdp2_mt.band[0] = *cliprect;
	slock_lock(vdp2_mt.lock);
	vdp2_mt.bitmap = bitmap;
	vdp2_mt.bands = bands;
	vdp2_mt.pending = bands - 1;
	vdp2_mt.gen++;
	scond_broadcast(vdp2_mt.start);
	slock_unlock(vdp2_mt.lock);

	vdp2_render_band(bitmap, cliprect, 0, bands);

	slock_lock(vdp2_mt.lock);
	while (vdp2_mt.pending)
		scond_wait(vdp2_mt.done, vdp2_mt.lock);
	slock_unlock(vdp2_mt.lock);

	/* the elements the strips decoded are up to date now */
	for (i = 0; i < bands; i++)
	{
		int k;
		for (k = 0; k < vdp2_band_decoded_count[i]; k++)
		{
			UINT32 e = vdp2_band_decoded[i][k];
			stv_gfx[e >> 24].dirty[e & 0xffffff] = 0;
		}
		vdp2_band_decoded_count[i] = 0;
	}
	vdp2_render_mode = VDP2_RENDER_NORMAL;
	return 1;
}
#endif

/* the VDP2 screen, drawn at the end of the active display as MAME does */
static void vdp2_render(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	vdp2_window_cache_gen++;
	vdp2_fade_effects();

	vdp2_draw_back(bitmap,cliprect);

	if(get_disp())
	{
		memset(vdp1_sprite_priorities_in_fb_line, 0, sizeof(vdp1_sprite_priorities_in_fb_line));
		memset(vdp1_sprite_priorities_min_x, 0xff, sizeof(vdp1_sprite_priorities_min_x));
		memset(vdp1_sprite_priorities_max_x, 0, sizeof(vdp1_sprite_priorities_max_x));

#ifdef STV_VIDEO_THREADS
		/* strips of lines (of an even number of lines from an even one: an interlaced framebuffer line is two) */
		if (options.stv_video_threads > 1 && !(cliprect->min_y & 1) && cliprect->max_y - cliprect->min_y >= 64 &&
			vdp2_render_threaded(bitmap, cliprect))
			return;
#endif
		vdp2_render_layers(bitmap, cliprect, 1);
	}
}


/***************************************************************************
    VDP2 CRTC (MAME's saturn_vdp2.cpp): TV mode, status, H/V counters
***************************************************************************/

#define STV_LINES_PER_FRAME     263
#define STV_FRAMES_PER_SECOND   60

static int crtc_line;           /* the current line, from the driver */
static double crtc_line_time;   /* when it started */
static UINT8 true_vcount[263][4];
static UINT16 true_vcount_hi[263][4];
static struct mame_bitmap *vdp2_screen;
static struct rectangle vdp2_visarea;

static void crtc_init_vcounter_table(void)
{
	int i;

	/* 224 mode */
	for (i = 0; i < 263; i++)
	{
		int v = i;
		if (i > 0xec)
			v += 0xf9;
		true_vcount[i][0] = v & 0xff;
		true_vcount_hi[i][0] = v;
	}
	/* 240 mode */
	for (i = 0; i < 263; i++)
	{
		int v = i;
		if (i > 0xf5)
			v += 0xf9;
		true_vcount[i][1] = v & 0xff;
		true_vcount_hi[i][1] = v;
	}
	/* 256 modes (PAL only) */
	for (i = 0; i < 263; i++)
	{
		true_vcount_hi[i][2] = i;
		true_vcount_hi[i][3] = i;
	}
}

static int crtc_htotal(void)
{
	int res = BIT(crtc.hreso, 0) ? 455 : 427;

	/* x2 horizontal resolution in 640/704 modes */
	if (BIT(crtc.hreso, 1))
		res <<= 1;
	return res;
}

static void crtc_reconfigure(void)
{
	static const int d_vres[4] = { 224, 240, 256, 256 };
	static const int d_hres[4] = { 320, 352, 640, 704 };
	int vres_mask = (crtc.is_pal << 1) | 1;
	int horz_res, vert_res;

	/* reset odd bit if a dynamic resolution change occurs, stv:seabass cares */
	crtc.odd_bit = 1;

	vert_res = d_vres[crtc.vreso & vres_mask];

	/* double density interlace */
	if (crtc.lsmd == 3)
		vert_res *= 2;

	horz_res = d_hres[crtc.hreso & 3];

	/* exclusive modes (31kHz and Hi-Vision) are 480 lines whatever VRESO says */
	if (BIT(crtc.hreso, 2))
		vert_res = 480;

	crtc.hdisplay = horz_res;
	crtc.vdisplay = vert_res;

	rect_set(&vdp2_visarea, 0, horz_res - 1, 0, vert_res - 1);
}

/* the horizontal position in pixel clocks of the current mode */
static int crtc_hpos(void)
{
	double line_period = 1.0 / (STV_FRAMES_PER_SECOND * STV_LINES_PER_FRAME);
	double elapsed = timer_get_time() - crtc_line_time;
	int htotal = crtc_htotal();
	int hpos = (int)(elapsed / line_period * htotal);

	if (hpos < 0)
		hpos = 0;
	if (hpos >= htotal)
		hpos = htotal - 1;
	return hpos;
}

static int crtc_get_hcounter(void)
{
	int hcount = crtc_hpos();

	switch (crtc.hreso & 6)
	{
		case 0: hcount &= 0x1ff; hcount <<= 1; break;  /* normal */
		case 2: hcount &= 0x3ff; break;                 /* hi-res */
		case 4: hcount &= 0x1ff; break;                 /* exclusive normal */
		case 6: hcount >>= 1; hcount &= 0x1ff; break;   /* exclusive hi-res */
	}
	return hcount;
}

static int crtc_get_vcounter(void)
{
	int vcount = crtc_line;

	/* exclusive monitor */
	if (BIT(crtc.hreso, 2))
		return vcount & 0x3ff;

	/* double density interlace: the field is the low bit */
	if (crtc.lsmd == 3)
		return ((vcount << 1) & ~1) | (crtc.odd_bit ^ 1);

	return true_vcount_hi[vcount % 263][crtc.vreso & 3];
}

static int crtc_get_hblank(void)
{
	return crtc_hpos() > crtc.hdisplay - 1;
}

int stv_vdp2_vblank_start_line(void)
{
	/* the first setting is at 240, the 16 lines are border overscan */
	static const int d_vres[4] = { 240, 240, 256, 256 };
	return d_vres[crtc.vreso & ((crtc.is_pal << 1) | 1)];
}

static int crtc_get_vblank(void)
{
	return crtc_line >= stv_vdp2_vblank_start_line();
}

static void crtc_tvmd_w(UINT16 data, UINT16 mem_mask)
{
	COMBINE_DATA(&crtc.tvmd);
	crtc.disp = BIT(crtc.tvmd, 15);
	crtc.bdclmd = BIT(crtc.tvmd, 8);
	crtc.lsmd = (crtc.tvmd >> 6) & 3;
	crtc.vreso = (crtc.tvmd >> 4) & 3;
	crtc.hreso = (crtc.tvmd >> 0) & 7;
	if (!(mem_mask & 0x00ff) && (crtc.tvmd & 0xff) != (crtc.old_tvmd & 0xff))
		crtc_reconfigure();
	crtc.old_tvmd = crtc.tvmd;
}

static UINT16 crtc_regs_r(int offset)
{
	switch (offset)
	{
		case 0x00/2:    /* TVMD */
			return crtc.tvmd;

		case 0x02/2:    /* EXTEN: reading it latches the H/V counters */
			if (!crtc.exlten)
			{
				crtc.hcounter_latch = crtc_get_hcounter();
				crtc.vcounter_latch = crtc_get_vcounter();
				crtc.exltfg |= 1;
			}
			return crtc.exten;

		case 0x04/2:    /* TVSTAT */
		{
			/* exclusive modes force the odd bit; vblank reads 1 with the display off */
			int odd_flag = crtc.odd_bit | BIT(crtc.hreso, 2);
			int vblank_flag = crtc_get_vblank() | (!crtc.disp);
			UINT16 res = (crtc.exltfg << 9) | (crtc.exsyfg << 8) | (vblank_flag << 3)
					| (crtc_get_hblank() << 2) | (odd_flag << 1) | crtc.is_pal;

			crtc.exltfg &= ~1;
			crtc.exsyfg &= ~1;
			return res;
		}

		case 0x06/2:    /* VRSIZE */
			return (crtc.vramsz << 15) | 0;

		case 0x08/2:    /* HCNT */
			return crtc.hcounter_latch;

		case 0x0a/2:    /* VCNT */
			return crtc.vcounter_latch;
	}
	return vdp2_regs_r(offset);
}

static void crtc_regs_w(int offset, UINT16 data, UINT16 mem_mask)
{
	switch (offset)
	{
		case 0x00/2:
			crtc_tvmd_w(data, mem_mask);
			break;

		case 0x02/2:
			COMBINE_DATA(&crtc.exten);
			crtc.exlten = BIT(crtc.exten, 9);
			crtc.exsyen = BIT(crtc.exten, 8);
			crtc.dasel = BIT(crtc.exten, 1);
			crtc.exbgen = BIT(crtc.exten, 0);
			break;

		case 0x06/2:
			if (!(mem_mask & 0xff00))
				crtc.vramsz = BIT(data, 15);
			break;
	}
	/* the VDP2 code also reads the registers */
	vdp2_regs_w(offset, data, mem_mask);
}

static void crtc_state_save_postload(void)
{
	crtc_reconfigure();
}

static void crtc_start(void)
{
	memset(&crtc, 0, sizeof(crtc));
	crtc_init_vcounter_table();
	crtc.odd_bit = 1;
	crtc.old_tvmd = 0xffff;
	crtc.is_pal = 0;
	crtc_line = 0;
	crtc_line_time = 0;
	crtc_reconfigure();

	state_save_register_UINT16("stv_crtc", 0, "tvmd", &crtc.tvmd, 1);
	state_save_register_UINT16("stv_crtc", 0, "old_tvmd", &crtc.old_tvmd, 1);
	state_save_register_int("stv_crtc", 0, "disp", &crtc.disp);
	state_save_register_int("stv_crtc", 0, "bdclmd", &crtc.bdclmd);
	state_save_register_int("stv_crtc", 0, "lsmd", &crtc.lsmd);
	state_save_register_int("stv_crtc", 0, "vreso", &crtc.vreso);
	state_save_register_int("stv_crtc", 0, "hreso", &crtc.hreso);
	state_save_register_int("stv_crtc", 0, "odd_bit", &crtc.odd_bit);
	state_save_register_UINT16("stv_crtc", 0, "exten", &crtc.exten, 1);
	state_save_register_int("stv_crtc", 0, "exlten", &crtc.exlten);
	state_save_register_int("stv_crtc", 0, "exsyen", &crtc.exsyen);
	state_save_register_int("stv_crtc", 0, "dasel", &crtc.dasel);
	state_save_register_int("stv_crtc", 0, "exbgen", &crtc.exbgen);
	state_save_register_int("stv_crtc", 0, "exltfg", &crtc.exltfg);
	state_save_register_int("stv_crtc", 0, "exsyfg", &crtc.exsyfg);
	state_save_register_UINT16("stv_crtc", 0, "hcnt", &crtc.hcounter_latch, 1);
	state_save_register_UINT16("stv_crtc", 0, "vcnt", &crtc.vcounter_latch, 1);
	state_save_register_int("stv_crtc", 0, "vramsz", &crtc.vramsz);
	state_save_register_func_postload(crtc_state_save_postload);
}


/***************************************************************************
    Frame timing, from the driver
***************************************************************************/

/* the start of a line (0 to 262) */
void stv_vdp2_scanline(int line)
{
	crtc_line = line;
	crtc_line_time = timer_get_time();

	/* the screen is drawn at the end of the active display */
	{
		int last = crtc.vdisplay;
		if (crtc.lsmd == 3 || BIT(crtc.hreso, 2))
			last /= 2;
		if (last > STV_LINES_PER_FRAME - 1)
			last = STV_LINES_PER_FRAME - 1;
		if (line == last)
			vdp2_render(vdp2_screen, &vdp2_visarea);
	}

	/* the odd bit flips at vblank-in */
	if (line == stv_vdp2_vblank_start_line())
		crtc.odd_bit ^= 1;
}

void stv_vdp1_video_update(void)
{
	vdp1_video_update();
}

void stv_vdp1_vblank_erase(void)
{
	/* the docs say VBE happens one line after vblank-in */
	if (VDP1_VBE())
		vdp1.framebuffer_clear_on_next_frame = 1;
}


/***************************************************************************
    Memory handlers (the 16-bit registers as 32-bit pairs, big endian)
***************************************************************************/

READ32_HANDLER( stv_vdp1_regs_r )
{
	UINT32 res = 0;
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		res |= vdp1_regs_r(offset*2) << 16;
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		res |= vdp1_regs_r(offset*2+1);
	return res;
}

WRITE32_HANDLER( stv_vdp1_regs_w )
{
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		vdp1_regs_w(offset*2, data >> 16, mem_mask >> 16);
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		vdp1_regs_w(offset*2+1, data & 0xffff, mem_mask & 0xffff);
}

READ32_HANDLER( stv_vdp1_vram_r )
{
	return vdp1_vram_r(offset);
}

WRITE32_HANDLER( stv_vdp1_vram_w )
{
	vdp1_vram_w(offset, data, mem_mask);
}

READ32_HANDLER( stv_vdp1_framebuffer0_r )
{
	return vdp1_framebuffer0_r(offset, mem_mask);
}

WRITE32_HANDLER( stv_vdp1_framebuffer0_w )
{
	vdp1_framebuffer0_w(offset, data, mem_mask);
}

READ32_HANDLER( stv_vdp2_vram_r )
{
	return vdp2_vram_r(offset & 0x7ffff/4);
}

WRITE32_HANDLER( stv_vdp2_vram_w )
{
	vdp2_vram_w(offset & 0x7ffff/4, data, mem_mask);
}

READ32_HANDLER( stv_vdp2_cram_r )
{
	return vdp2_cram_r(offset);
}

WRITE32_HANDLER( stv_vdp2_cram_w )
{
	vdp2_cram_w(offset, data, mem_mask);
}

READ32_HANDLER( stv_vdp2_regs_r )
{
	UINT32 res = 0;
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		res |= crtc_regs_r(offset*2) << 16;
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		res |= crtc_regs_r(offset*2+1);
	return res;
}

WRITE32_HANDLER( stv_vdp2_regs_w )
{
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		crtc_regs_w(offset*2, data >> 16, mem_mask >> 16);
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		crtc_regs_w(offset*2+1, data & 0xffff, mem_mask & 0xffff);
}


/***************************************************************************
    Video start, stop and update
***************************************************************************/

VIDEO_START( stv_vdp2 )
{
	if (vdp2_start() || vdp1_start())
		return 1;
	crtc_start();
	m_vdpdebug_roz = 0;
	{
		int w, y;
		for (w = 0; w < 2; w++)
			for (y = 0; y < VDP2_WINDOW_LINES; y++)
				vdp2_window_coords[w][y].gen = -1;
		for (y = 0; y < VDP2_WINDOW_LINES; y++)
			vdp2_window_spans[y].gen = -1;
	}

	/* the screen is drawn here during the frame, then copied */
	vdp2_screen = auto_bitmap_alloc_depth(704, 512, 32);
	if (!vdp2_screen)
		return 1;
	return 0;
}

VIDEO_STOP( stv_vdp2 )
{
	int i;
#ifdef STV_VIDEO_THREADS
	vdp2_threads_stop();
#endif
	for (i = 0; i < 2; i++)
		if (vdp2l.roz_bitmap[i])
		{
			bitmap_free(vdp2l.roz_bitmap[i]);
			vdp2l.roz_bitmap[i] = NULL;
		}
}

VIDEO_UPDATE( stv_vdp2 )
{
	if (Machine->visible_area.max_x != vdp2_visarea.max_x || Machine->visible_area.max_y != vdp2_visarea.max_y)
		set_visible_area(0, vdp2_visarea.max_x, 0, vdp2_visarea.max_y);

	copybitmap(bitmap, vdp2_screen, 0, 0, 0, 0, &vdp2_visarea, TRANSPARENCY_NONE, 0);
	stv_draw_crosshairs(bitmap, &vdp2_visarea);
}
