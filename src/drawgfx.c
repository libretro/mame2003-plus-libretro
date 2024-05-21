#ifndef DECLARE

#include "driver.h"


#ifdef MSB_FIRST
#define SHIFT0 24
#define SHIFT1 16
#define SHIFT2 8
#define SHIFT3 0
#else
#define SHIFT0 0
#define SHIFT1 8
#define SHIFT2 16
#define SHIFT3 24
#endif


typedef void (*plot_pixel_proc)(struct mame_bitmap *bitmap,int x,int y,pen_t pen);
typedef pen_t (*read_pixel_proc)(struct mame_bitmap *bitmap,int x,int y);
typedef void (*plot_box_proc)(struct mame_bitmap *bitmap,int x,int y,int width,int height,pen_t pen);


UINT8 gfx_drawmode_table[256];
UINT8 gfx_alpharange_table[256];

static UINT8 is_raw[TRANSPARENCY_MODES];


#ifdef ALIGN_INTS /* GSL 980108 read/write nonaligned dword routine for ARM processor etc */

static INLINE UINT32 read_dword(void *address)
{
	if ((long)address & 3)
	{
  		return	(*((UINT8 *)address  ) << SHIFT0) +
				(*((UINT8 *)address+1) << SHIFT1) +
				(*((UINT8 *)address+2) << SHIFT2) +
				(*((UINT8 *)address+3) << SHIFT3);
	}
	else
		return *(UINT32 *)address;
}


static INLINE void write_dword(void *address, UINT32 data)
{
  	if ((long)address & 3)
	{
		*((UINT8 *)address)   = (data>>SHIFT0);
		*((UINT8 *)address+1) = (data>>SHIFT1);
		*((UINT8 *)address+2) = (data>>SHIFT2);
		*((UINT8 *)address+3) = (data>>SHIFT3);
		return;
  	}
  	else
		*(UINT32 *)address = data;
}
#else
#define read_dword(address) *(int *)address
#define write_dword(address,data) *(int *)address=data
#endif



static INLINE int readbit(const UINT8 *src,int bitnum)
{
	return src[bitnum / 8] & (0x80 >> (bitnum % 8));
}

struct _alpha_cache alpha_cache;
int alpha_active;

void alpha_init(void)
{
	int lev, byte;
	for(lev=0; lev<257; lev++)
		for(byte=0; byte<256; byte++)
			alpha_cache.alpha[lev][byte] = (byte*lev) >> 8;
	alpha_set_level(255);
}


static void calc_penusage(struct GfxElement *gfx,int num)
{
	int x,y;
	UINT8 *dp;

	if (!gfx->pen_usage) return;

	/* fill the pen_usage array with info on the used pens */
	gfx->pen_usage[num] = 0;

	dp = gfx->gfxdata + num * gfx->char_modulo;

	if (gfx->flags & GFX_PACKED)
	{
		for (y = 0;y < gfx->height;y++)
		{
			for (x = 0;x < gfx->width/2;x++)
			{
				gfx->pen_usage[num] |= 1 << (dp[x] & 0x0f);
				gfx->pen_usage[num] |= 1 << (dp[x] >> 4);
			}
			dp += gfx->line_modulo;
		}
	}
	else
	{
		for (y = 0;y < gfx->height;y++)
		{
			for (x = 0;x < gfx->width;x++)
			{
				gfx->pen_usage[num] |= 1 << dp[x];
			}
			dp += gfx->line_modulo;
		}
	}
}

void decodechar(struct GfxElement *gfx,int num,const UINT8 *src,const struct GfxLayout *gl)
{
	int plane,x,y;
	int baseoffs;
	const UINT32 *xoffset = gl->xoffset;
	const UINT32 *yoffset = gl->yoffset;
	UINT8 *dp = gfx->gfxdata + num * gfx->char_modulo;

	memset(dp,0,gfx->char_modulo);

	baseoffs = num * gl->charincrement;

	if (gfx->flags & GFX_PACKED)
	{
		for (plane = 0;plane < gl->planes;plane++)
		{
			int shiftedbit = 1 << (gl->planes-1-plane);
			int offs = baseoffs + gl->planeoffset[plane];

			dp = gfx->gfxdata + num * gfx->char_modulo + (gfx->height-1) * gfx->line_modulo;

			y = gfx->height;
			while (--y >= 0)
			{
				int offs2 = offs + yoffset[y];

				x = gfx->width/2;
				while (--x >= 0)
				{
					if (readbit(src,offs2 + xoffset[2*x+1]))
						dp[x] |= shiftedbit << 4;
					if (readbit(src,offs2 + xoffset[2*x]))
						dp[x] |= shiftedbit;
				}
				dp -= gfx->line_modulo;
			}
		}
	}
	else
	{
		for (plane = 0;plane < gl->planes;plane++)
		{
			int shiftedbit = 1 << (gl->planes-1-plane);
			int offs = baseoffs + gl->planeoffset[plane];

			dp = gfx->gfxdata + num * gfx->char_modulo + (gfx->height-1) * gfx->line_modulo;

#ifdef PREROTATE_GFX
			y = gfx->height;
			while (--y >= 0)
			{
				int yoffs;

				yoffs = y;
				x = gfx->width;
				while (--x >= 0)
				{
					int xoffs;

					xoffs = x;
					if (readbit(src,offs + xoffset[xoffs] + yoffset[yoffs]))
						dp[x] |= shiftedbit;
				}
				dp -= gfx->line_modulo;
			}
#else
			y = gfx->height;
			while (--y >= 0)
			{
				int offs2 = offs + yoffset[y];

				x = gfx->width;
				while (--x >= 0)
				{
					if (readbit(src,offs2 + xoffset[x]))
						dp[x] |= shiftedbit;
				}
				dp -= gfx->line_modulo;
			}
#endif
		}
	}

	calc_penusage(gfx,num);
}


struct GfxElement *decodegfx(const UINT8 *src,const struct GfxLayout *gl)
{
	int c;
	struct GfxElement *gfx;


	if ((gfx = malloc(sizeof(struct GfxElement))) == 0)
		return 0;
	memset(gfx,0,sizeof(struct GfxElement));

	gfx->width = gl->width;
	gfx->height = gl->height;

	gfx->total_elements = gl->total;
	gfx->color_granularity = 1 << gl->planes;

	gfx->pen_usage = 0; /* need to make sure this is NULL if the next test fails) */
	if (gfx->color_granularity <= 32)	/* can't handle more than 32 pens */
		gfx->pen_usage = malloc(gfx->total_elements * sizeof(int));
		/* no need to check for failure, the code can work without pen_usage */

	if (gl->planeoffset[0] == GFX_RAW)
	{
		if (gl->planes <= 4) gfx->flags |= GFX_PACKED;

		gfx->line_modulo = gl->yoffset[0] / 8;
		gfx->char_modulo = gl->charincrement / 8;

		gfx->gfxdata = (UINT8 *)src + gl->xoffset[0] / 8;
		gfx->flags |= GFX_DONT_FREE_GFXDATA;

		for (c = 0;c < gfx->total_elements;c++)
			calc_penusage(gfx,c);
	}
	else
	{
		if (0 && gl->planes <= 4 && !(gfx->width & 1))
/*		if (gl->planes <= 4 && !(gfx->width & 1))*/
		{
			gfx->flags |= GFX_PACKED;
			gfx->line_modulo = gfx->width/2;
		}
		else
			gfx->line_modulo = gfx->width;
		gfx->char_modulo = gfx->line_modulo * gfx->height;

		if ((gfx->gfxdata = malloc(gfx->total_elements * gfx->char_modulo * sizeof(UINT8))) == 0)
		{
			free(gfx->pen_usage);
			free(gfx);
			return 0;
		}

		for (c = 0;c < gfx->total_elements;c++)
			decodechar(gfx,c,src,gl);
	}

	return gfx;
}


void freegfx(struct GfxElement *gfx)
{
	if (gfx)
	{
		free(gfx->pen_usage);
		if (!(gfx->flags & GFX_DONT_FREE_GFXDATA))
			free(gfx->gfxdata);
		free(gfx);
	}
}




static INLINE void blockmove_NtoN_transpen_noremap8(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT8 *dstdata,int dstmodulo,
		int transpen)
{
	UINT8 *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4++)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if( (xod4&0x000000ff) && (xod4&0x0000ff00) &&
					(xod4&0x00ff0000) && (xod4&0xff000000) )
				{
					write_dword((UINT32 *)dstdata,col4);
				}
				else
				{
					if (xod4 & (0xff<<SHIFT0)) dstdata[0] = col4>>SHIFT0;
					if (xod4 & (0xff<<SHIFT1)) dstdata[1] = col4>>SHIFT1;
					if (xod4 & (0xff<<SHIFT2)) dstdata[2] = col4>>SHIFT2;
					if (xod4 & (0xff<<SHIFT3)) dstdata[3] = col4>>SHIFT3;
				}
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

static INLINE void blockmove_NtoN_transpen_noremap_flipx8(
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT8 *dstdata,int dstmodulo,
		int transpen)
{
	UINT8 *end;
	int trans4;
	UINT32 *sd4;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/
	srcdata -= 3;

	trans4 = transpen * 0x01010101;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (((long)srcdata & 3) && dstdata < end)	/* longword align */
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = col;
			dstdata++;
		}
		sd4 = (UINT32 *)srcdata;
		while (dstdata <= end - 4)
		{
			UINT32 col4;

			if ((col4 = *(sd4--)) != trans4)
			{
				UINT32 xod4;

				xod4 = col4 ^ trans4;
				if (xod4 & (0xff<<SHIFT0)) dstdata[3] = (col4>>SHIFT0);
				if (xod4 & (0xff<<SHIFT1)) dstdata[2] = (col4>>SHIFT1);
				if (xod4 & (0xff<<SHIFT2)) dstdata[1] = (col4>>SHIFT2);
				if (xod4 & (0xff<<SHIFT3)) dstdata[0] = (col4>>SHIFT3);
			}
			dstdata += 4;
		}
		srcdata = (UINT8 *)sd4;
		while (dstdata < end)
		{
			int col;

			col = srcdata[3];
			srcdata--;
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}


static INLINE void blockmove_NtoN_transpen_noremap16(
		const UINT16 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT16 *dstdata,int dstmodulo,
		int transpen)
{
	UINT16 *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

static INLINE void blockmove_NtoN_transpen_noremap_flipx16(
		const UINT16 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT16 *dstdata,int dstmodulo,
		int transpen)
{
	UINT16 *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata--);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

static INLINE void blockmove_NtoN_transpen_noremap32(
		const UINT32 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT32 *dstdata,int dstmodulo,
		int transpen)
{
	UINT32 *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata++);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}

static INLINE void blockmove_NtoN_transpen_noremap_flipx32(
		const UINT32 *srcdata,int srcwidth,int srcheight,int srcmodulo,
		UINT32 *dstdata,int dstmodulo,
		int transpen)
{
	UINT32 *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata < end)
		{
			int col;

			col = *(srcdata--);
			if (col != transpen) *dstdata = col;
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
}



static int afterdrawmask = 31;
int pdrawgfx_shadow_lowpri = 0;


/* 8-bit version */
#define DATA_TYPE UINT8
#define DEPTH 8

#define DECLARE(function,args,body)
#define DECLAREG(function,args,body)

#define HMODULO 1
#define VMODULO dstmodulo
#define COMMON_ARGS														\
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,	\
		int leftskip,int topskip,int flipx,int flipy,					\
		DATA_TYPE *dstdata,int dstwidth,int dstheight,int dstmodulo

#define COLOR_ARG unsigned int colorbase,UINT8 *pridata,UINT32 pmask
#define INCREMENT_DST(n) {dstdata+=(n);pridata += (n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) { if (((1 << (pridata[dest] & 0x1f)) & pmask) == 0) { if (pridata[dest] & 0x80) { dstdata[dest] = palette_shadow_table[n];} else { dstdata[dest] = (n);} } pridata[dest] = (pridata[dest] & 0x7f) | afterdrawmask; }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw_pri8 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata,UINT8 *pridata,UINT32 pmask
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) { if (((1 << (pridata[dest] & 0x1f)) & pmask) == 0) { if (pridata[dest] & 0x80) { dstdata[dest] = palette_shadow_table[n];} else { dstdata[dest] = (n);} } pridata[dest] = (pridata[dest] & 0x7f) | afterdrawmask; }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_pri8 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#define COLOR_ARG unsigned int colorbase
#define INCREMENT_DST(n) {dstdata+=(n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw8 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##8 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#undef HMODULO
#undef VMODULO
#undef COMMON_ARGS
#undef DECLARE
#undef DECLAREG

#define DECLARE(function,args,body) void function##8 args body
#define DECLAREG(function,args,body) void function##8 args body
#define DECLARE_SWAP_RAW_PRI(function,args,body)
#define BLOCKMOVE(function,flipx,args) \
	if (flipx) blockmove_##function##_flipx##8 args ; \
	else blockmove_##function##8 args
#define BLOCKMOVELU(function,args) \
	blockmove_##function##8 args
#define BLOCKMOVERAW(function,args) \
	blockmove_##function##_raw##8 args
#define BLOCKMOVEPRI(function,args) \
	blockmove_##function##_pri##8 args
#define BLOCKMOVERAWPRI(function,args) \
	blockmove_##function##_raw_pri##8 args
#include "drawgfx.c"
#undef DECLARE
#undef DECLARE_SWAP_RAW_PRI
#undef DECLAREG
#undef BLOCKMOVE
#undef BLOCKMOVELU
#undef BLOCKMOVERAW
#undef BLOCKMOVEPRI
#undef BLOCKMOVERAWPRI

#undef DEPTH
#undef DATA_TYPE

/* 16-bit version */
#define DATA_TYPE UINT16
#define DEPTH 16
#define alpha_blend_r alpha_blend_r16
#define alpha_blend alpha_blend16

#define DECLARE(function,args,body)
#define DECLAREG(function,args,body)

#define HMODULO 1
#define VMODULO dstmodulo
#define COMMON_ARGS														\
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,	\
		int leftskip,int topskip,int flipx,int flipy,					\
		DATA_TYPE *dstdata,int dstwidth,int dstheight,int dstmodulo

#define COLOR_ARG unsigned int colorbase,UINT8 *pridata,UINT32 pmask
#define INCREMENT_DST(n) {dstdata+=(n);pridata += (n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) { if (((1 << (pridata[dest] & 0x1f)) & pmask) == 0) { if (pridata[dest] & 0x80) { dstdata[dest] = palette_shadow_table[n];} else { dstdata[dest] = (n);} } pridata[dest] = (pridata[dest] & 0x7f) | afterdrawmask; }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw_pri16 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata,UINT8 *pridata,UINT32 pmask
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) { if (((1 << (pridata[dest] & 0x1f)) & pmask) == 0) { if (pridata[dest] & 0x80) { dstdata[dest] = palette_shadow_table[n];} else { dstdata[dest] = (n);} } pridata[dest] = (pridata[dest] & 0x7f) | afterdrawmask; }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_pri16 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#define COLOR_ARG unsigned int colorbase
#define INCREMENT_DST(n) {dstdata+=(n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw16 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##16 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#undef HMODULO
#undef VMODULO
#undef COMMON_ARGS
#undef DECLARE
#undef DECLAREG

#define DECLARE(function,args,body) void function##16 args body
#define DECLAREG(function,args,body) void function##16 args body
#define DECLARE_SWAP_RAW_PRI(function,args,body)
#define BLOCKMOVE(function,flipx,args) \
	if (flipx) blockmove_##function##_flipx##16 args ; \
	else blockmove_##function##16 args
#define BLOCKMOVELU(function,args) \
	blockmove_##function##16 args
#define BLOCKMOVERAW(function,args) \
	blockmove_##function##_raw##16 args
#define BLOCKMOVEPRI(function,args) \
	blockmove_##function##_pri##16 args
#define BLOCKMOVERAWPRI(function,args) \
	blockmove_##function##_raw_pri##16 args
#include "drawgfx.c"
#undef DECLARE
#undef DECLARE_SWAP_RAW_PRI
#undef DECLAREG
#undef BLOCKMOVE
#undef BLOCKMOVELU
#undef BLOCKMOVERAW
#undef BLOCKMOVEPRI
#undef BLOCKMOVERAWPRI

#undef DEPTH
#undef DATA_TYPE
#undef alpha_blend_r
#undef alpha_blend

/* 32-bit version */
/** AAT032503: added limited 32-bit shadow and highlight support*/
static INLINE UINT32 SHADOW32(UINT32 c) {
	c = (c>>9&0x7c00) | (c>>6&0x03e0) | (c>>3&0x001f);
	return(((UINT32*)palette_shadow_table)[c]); }

#define DATA_TYPE UINT32
#define DEPTH 32
#define alpha_blend_r alpha_blend_r32
#define alpha_blend alpha_blend32

#define DECLARE(function,args,body)
#define DECLAREG(function,args,body)

#define HMODULO 1
#define VMODULO dstmodulo
#define COMMON_ARGS														\
		const UINT8 *srcdata,int srcwidth,int srcheight,int srcmodulo,	\
		int leftskip,int topskip,int flipx,int flipy,					\
		DATA_TYPE *dstdata,int dstwidth,int dstheight,int dstmodulo

#define COLOR_ARG unsigned int colorbase,UINT8 *pridata,UINT32 pmask
#define INCREMENT_DST(n) {dstdata+=(n);pridata += (n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) { UINT8 r8=pridata[dest]; if(!(1<<(r8&0x1f)&pmask)){ if(afterdrawmask){ r8&=0x7f; r8|=0x1f; dstdata[dest]=(n); pridata[dest]=r8; } else if(!(r8&0x80)){ dstdata[dest]=SHADOW32(n); pridata[dest]|=0x80; } } }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw_pri32 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata,UINT8 *pridata,UINT32 pmask
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) { UINT8 r8=pridata[dest]; if(!(1<<(r8&0x1f)&pmask)){ if(afterdrawmask){ r8&=0x7f; r8|=0x1f; dstdata[dest]=(n); pridata[dest]=r8; } else if(!(r8&0x80)){ dstdata[dest]=SHADOW32(n); pridata[dest]|=0x80; } } }
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_pri32 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#define COLOR_ARG unsigned int colorbase
#define INCREMENT_DST(n) {dstdata+=(n);}
#define LOOKUP(n) (colorbase + (n))
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##_raw32 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef SETPIXELCOLOR

#define COLOR_ARG const pen_t *paldata
#define LOOKUP(n) (paldata[n])
#define SETPIXELCOLOR(dest,n) {dstdata[dest] = (n);}
#define DECLARE_SWAP_RAW_PRI(function,args,body) void function##32 args body
#include "drawgfx.c"
#undef DECLARE_SWAP_RAW_PRI
#undef COLOR_ARG
#undef LOOKUP
#undef INCREMENT_DST
#undef SETPIXELCOLOR

#undef HMODULO
#undef VMODULO
#undef COMMON_ARGS
#undef DECLARE
#undef DECLAREG

#define DECLARE(function,args,body) void function##32 args body
#define DECLAREG(function,args,body) void function##32 args body
#define DECLARE_SWAP_RAW_PRI(function,args,body)
#define BLOCKMOVE(function,flipx,args) \
	if (flipx) blockmove_##function##_flipx##32 args ; \
	else blockmove_##function##32 args
#define BLOCKMOVELU(function,args) \
	blockmove_##function##32 args
#define BLOCKMOVERAW(function,args) \
	blockmove_##function##_raw##32 args
#define BLOCKMOVEPRI(function,args) \
	blockmove_##function##_pri##32 args
#define BLOCKMOVERAWPRI(function,args) \
	blockmove_##function##_raw_pri##32 args
#include "drawgfx.c"
#undef DECLARE
#undef DECLARE_SWAP_RAW_PRI
#undef DECLAREG
#undef BLOCKMOVE
#undef BLOCKMOVELU
#undef BLOCKMOVERAW
#undef BLOCKMOVEPRI
#undef BLOCKMOVERAWPRI

#undef DEPTH
#undef DATA_TYPE
#undef alpha_blend_r
#undef alpha_blend


/***************************************************************************

  Draw graphic elements in the specified bitmap.

  transparency == TRANSPARENCY_NONE - no transparency.
  transparency == TRANSPARENCY_PEN - bits whose _original_ value is == transparent_color
                                     are transparent. This is the most common kind of
									 transparency.
  transparency == TRANSPARENCY_PENS - as above, but transparent_color is a mask of
  									 transparent pens.
  transparency == TRANSPARENCY_COLOR - bits whose _remapped_ palette index (taken from
                                     Machine->game_colortable) is == transparent_color

  transparency == TRANSPARENCY_PEN_TABLE - the transparency condition is same as TRANSPARENCY_PEN
					A special drawing is done according to gfx_drawmode_table[source pixel].
					DRAWMODE_NONE      transparent
					DRAWMODE_SOURCE    normal, draw source pixel.
					DRAWMODE_SHADOW    destination is changed through palette_shadow_table[]

***************************************************************************/

static INLINE void common_drawgfx(struct mame_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		struct mame_bitmap *pri_buffer,UINT32 pri_mask)
{
	if (!gfx)
	{
		usrintf_showmessage("drawgfx() gfx == 0");
		return;
	}
	if (!gfx->colortable && !is_raw[transparency])
	{
		usrintf_showmessage("drawgfx() gfx->colortable == 0");
		return;
	}

	code %= gfx->total_elements;
	if (!is_raw[transparency])
		color %= gfx->total_colors;

	if (!alpha_active && (transparency == TRANSPARENCY_ALPHAONE || transparency == TRANSPARENCY_ALPHA || transparency == TRANSPARENCY_ALPHARANGE))
	{
		if (transparency == TRANSPARENCY_ALPHAONE && (cpu_getcurrentframe() & 1))
		{
			transparency = TRANSPARENCY_PENS;
			transparent_color = (1 << (transparent_color & 0xff))|(1 << (transparent_color >> 8));
		}
		else
		{
			transparency = TRANSPARENCY_PEN;
			transparent_color &= 0xff;
		}
	}

	if (gfx->pen_usage && (transparency == TRANSPARENCY_PEN || transparency == TRANSPARENCY_PENS))
	{
		int transmask = 0;

		if (transparency == TRANSPARENCY_PEN)
		{
			transmask = 1 << (transparent_color & 0xff);
		}
		else	/* transparency == TRANSPARENCY_PENS */
		{
			transmask = transparent_color;
		}

		if ((gfx->pen_usage[code] & ~transmask) == 0)
			/* character is totally transparent, no need to draw */
			return;
		else if ((gfx->pen_usage[code] & transmask) == 0)
			/* character is totally opaque, can disable transparency */
			transparency = TRANSPARENCY_NONE;
	}

	if (dest->depth == 8)
		drawgfx_core8(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
	else if(dest->depth == 15 || dest->depth == 16)
		drawgfx_core16(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
	else
		drawgfx_core32(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
}

void drawgfx(struct mame_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfx(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,NULL,0);
	profiler_mark(PROFILER_END);
}

void pdrawgfx(struct mame_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfx(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,priority_bitmap,priority_mask | (1<<31));
	profiler_mark(PROFILER_END);
}

void mdrawgfx(struct mame_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfx(dest,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,priority_bitmap,priority_mask);
	profiler_mark(PROFILER_END);
}


/***************************************************************************

  Use drawgfx() to copy a bitmap onto another at the given position.
  This function will very likely change in the future.

***************************************************************************/
void copybitmap(struct mame_bitmap *dest,struct mame_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	/* translate to proper transparency here */
	if (transparency == TRANSPARENCY_NONE)
		transparency = TRANSPARENCY_NONE_RAW;
	else if (transparency == TRANSPARENCY_PEN)
		transparency = TRANSPARENCY_PEN_RAW;
	else if (transparency == TRANSPARENCY_COLOR)
	{
		transparent_color = Machine->pens[transparent_color];
		transparency = TRANSPARENCY_PEN_RAW;
	}

	copybitmap_remap(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);
}


void copybitmap_remap(struct mame_bitmap *dest,struct mame_bitmap *src,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	profiler_mark(PROFILER_COPYBITMAP);

	if (dest->depth == 8)
		copybitmap_core8(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);
	else if(dest->depth == 15 || dest->depth == 16)
		copybitmap_core16(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);
	else
		copybitmap_core32(dest,src,flipx,flipy,sx,sy,clip,transparency,transparent_color);

	profiler_mark(PROFILER_END);
}



/***************************************************************************

  Copy a bitmap onto another with scroll and wraparound.
  This function supports multiple independently scrolling rows/columns.
  "rows" is the number of indepentently scrolling rows. "rowscroll" is an
  array of integers telling how much to scroll each row. Same thing for
  "cols" and "colscroll".
  If the bitmap cannot scroll in one direction, set rows or columns to 0.
  If the bitmap scrolls as a whole, set rows and/or cols to 1.
  Bidirectional scrolling is, of course, supported only if the bitmap
  scrolls as a whole in at least one direction.

***************************************************************************/
void copyscrollbitmap(struct mame_bitmap *dest,struct mame_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	/* translate to proper transparency here */
	if (transparency == TRANSPARENCY_NONE)
		transparency = TRANSPARENCY_NONE_RAW;
	else if (transparency == TRANSPARENCY_PEN)
		transparency = TRANSPARENCY_PEN_RAW;
	else if (transparency == TRANSPARENCY_COLOR)
	{
		transparent_color = Machine->pens[transparent_color];
		transparency = TRANSPARENCY_PEN_RAW;
	}

	copyscrollbitmap_remap(dest,src,rows,rowscroll,cols,colscroll,clip,transparency,transparent_color);
}

void copyscrollbitmap_remap(struct mame_bitmap *dest,struct mame_bitmap *src,
		int rows,const int *rowscroll,int cols,const int *colscroll,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	int srcwidth,srcheight,destwidth,destheight;
	struct rectangle orig_clip;


	if (clip)
	{
		orig_clip.min_x = clip->min_x;
		orig_clip.max_x = clip->max_x;
		orig_clip.min_y = clip->min_y;
		orig_clip.max_y = clip->max_y;
	}
	else
	{
		orig_clip.min_x = 0;
		orig_clip.max_x = dest->width-1;
		orig_clip.min_y = 0;
		orig_clip.max_y = dest->height-1;
	}
	clip = &orig_clip;

	if (rows == 0 && cols == 0)
	{
		copybitmap(dest,src,0,0,0,0,clip,transparency,transparent_color);
		return;
	}

	profiler_mark(PROFILER_COPYBITMAP);

	srcwidth = src->width;
	srcheight = src->height;
	destwidth = dest->width;
	destheight = dest->height;

	if (rows == 0)
	{
		/* scrolling columns */
		int col,colwidth;
		struct rectangle myclip;


		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,0,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,0,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 0)
	{
		/* scrolling rows */
		int row,rowheight;
		struct rectangle myclip;


		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,0,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,0,&myclip,transparency,transparent_color);

			row += cons;
		}
	}
	else if (rows == 1 && cols == 1)
	{
		/* XY scrolling playfield */
		int scrollx,scrolly,sx,sy;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		for (sx = scrollx - srcwidth;sx < destwidth;sx += srcwidth)
			for (sy = scrolly - srcheight;sy < destheight;sy += srcheight)
				copybitmap(dest,src,0,0,sx,sy,clip,transparency,transparent_color);
	}
	else if (rows == 1)
	{
		/* scrolling columns + horizontal scroll */
		int col,colwidth;
		int scrollx;
		struct rectangle myclip;


		if (rowscroll[0] < 0) scrollx = srcwidth - (-rowscroll[0]) % srcwidth;
		else scrollx = rowscroll[0] % srcwidth;

		colwidth = srcwidth / cols;

		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		col = 0;
		while (col < cols)
		{
			int cons,scroll;


			/* count consecutive columns scrolled by the same amount */
			scroll = colscroll[col];
			cons = 1;
			while (col + cons < cols &&	colscroll[col + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcheight - (-scroll) % srcheight;
			else scroll %= srcheight;

			myclip.min_x = col * colwidth + scrollx;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx,scroll - srcheight,&myclip,transparency,transparent_color);

			myclip.min_x = col * colwidth + scrollx - srcwidth;
			if (myclip.min_x < clip->min_x) myclip.min_x = clip->min_x;
			myclip.max_x = (col + cons) * colwidth - 1 + scrollx - srcwidth;
			if (myclip.max_x > clip->max_x) myclip.max_x = clip->max_x;

			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scrollx - srcwidth,scroll - srcheight,&myclip,transparency,transparent_color);

			col += cons;
		}
	}
	else if (cols == 1)
	{
		/* scrolling rows + vertical scroll */
		int row,rowheight;
		int scrolly;
		struct rectangle myclip;


		if (colscroll[0] < 0) scrolly = srcheight - (-colscroll[0]) % srcheight;
		else scrolly = colscroll[0] % srcheight;

		rowheight = srcheight / rows;

		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;

		row = 0;
		while (row < rows)
		{
			int cons,scroll;


			/* count consecutive rows scrolled by the same amount */
			scroll = rowscroll[row];
			cons = 1;
			while (row + cons < rows &&	rowscroll[row + cons] == scroll)
				cons++;

			if (scroll < 0) scroll = srcwidth - (-scroll) % srcwidth;
			else scroll %= srcwidth;

			myclip.min_y = row * rowheight + scrolly;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly,&myclip,transparency,transparent_color);

			myclip.min_y = row * rowheight + scrolly - srcheight;
			if (myclip.min_y < clip->min_y) myclip.min_y = clip->min_y;
			myclip.max_y = (row + cons) * rowheight - 1 + scrolly - srcheight;
			if (myclip.max_y > clip->max_y) myclip.max_y = clip->max_y;

			copybitmap(dest,src,0,0,scroll,scrolly - srcheight,&myclip,transparency,transparent_color);
			copybitmap(dest,src,0,0,scroll - srcwidth,scrolly - srcheight,&myclip,transparency,transparent_color);

			row += cons;
		}
	}

	profiler_mark(PROFILER_END);
}


/* notes:
   - startx and starty MUST be UINT32 for calculations to work correctly
   - srcbitmap->width and height are assumed to be a power of 2 to speed up wraparound
   */
void copyrozbitmap(struct mame_bitmap *dest,struct mame_bitmap *src,
		UINT32 startx,UINT32 starty,int incxx,int incxy,int incyx,int incyy,int wraparound,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority)
{
	profiler_mark(PROFILER_COPYBITMAP);

	/* cheat, the core doesn't support TRANSPARENCY_NONE yet */
	if (transparency == TRANSPARENCY_NONE)
	{
		transparency = TRANSPARENCY_PEN;
		transparent_color = -1;
	}

	/* if necessary, remap the transparent color */
	if (transparency == TRANSPARENCY_COLOR)
	{
		transparency = TRANSPARENCY_PEN;
		transparent_color = Machine->pens[transparent_color];
	}

	if (transparency != TRANSPARENCY_PEN)
	{
		usrintf_showmessage("copyrozbitmap unsupported trans %02x",transparency);
		return;
	}

	if (dest->depth == 8)
		copyrozbitmap_core8(dest,src,startx,starty,incxx,incxy,incyx,incyy,wraparound,clip,transparency,transparent_color,priority);
	else if(dest->depth == 15 || dest->depth == 16)
		copyrozbitmap_core16(dest,src,startx,starty,incxx,incxy,incyx,incyy,wraparound,clip,transparency,transparent_color,priority);
	else
		copyrozbitmap_core32(dest,src,startx,starty,incxx,incxy,incyx,incyy,wraparound,clip,transparency,transparent_color,priority);

	profiler_mark(PROFILER_END);
}



/* fill a bitmap using the specified pen */
void fillbitmap(struct mame_bitmap *dest,pen_t pen,const struct rectangle *clip)
{
	int sx,sy,ex,ey,y;

	sx = 0;
	ex = dest->width - 1;
	sy = 0;
	ey = dest->height - 1;

	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	if (dest->depth == 32)
	{
		if (((pen >> 8) == (pen & 0xff)) && ((pen>>16) == (pen & 0xff)))
		{
			for (y = sy;y <= ey;y++)
				memset(((UINT32 *)dest->line[y]) + sx,pen&0xff,(ex-sx+1)*4);
		}
		else
		{
			UINT32 *sp = (UINT32 *)dest->line[sy];
			int x;

			for (x = sx;x <= ex;x++)
				sp[x] = pen;
			sp+=sx;
			for (y = sy+1;y <= ey;y++)
				memcpy(((UINT32 *)dest->line[y]) + sx,sp,(ex-sx+1)*4);
		}
	}
	else if (dest->depth == 15 || dest->depth == 16)
	{
		if ((pen >> 8) == (pen & 0xff))
		{
			for (y = sy;y <= ey;y++)
				memset(((UINT16 *)dest->line[y]) + sx,pen&0xff,(ex-sx+1)*2);
		}
		else
		{
			UINT16 *sp = (UINT16 *)dest->line[sy];
			int x;

			for (x = sx;x <= ex;x++)
				sp[x] = pen;
			sp+=sx;
			for (y = sy+1;y <= ey;y++)
				memcpy(((UINT16 *)dest->line[y]) + sx,sp,(ex-sx+1)*2);
		}
	}
	else
	{
		for (y = sy;y <= ey;y++)
			memset(((UINT8 *)dest->line[y]) + sx,pen,ex-sx+1);
	}
}



static INLINE void common_drawgfxzoom( struct mame_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		int scalex, int scaley,struct mame_bitmap *pri_buffer,UINT32 pri_mask)
{
	struct rectangle myclip;
	int alphapen = 0;

	UINT8 ah, al;

	al = (pdrawgfx_shadow_lowpri) ? 0 : 0x80;

	if (!scalex || !scaley) return;

	if (scalex == 0x10000 && scaley == 0x10000)
	{
		common_drawgfx(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,clip,transparency,transparent_color,pri_buffer,pri_mask);
		return;
	}

	if (transparency != TRANSPARENCY_PEN && transparency != TRANSPARENCY_PEN_RAW
			&& transparency != TRANSPARENCY_PENS && transparency != TRANSPARENCY_COLOR
			&& transparency != TRANSPARENCY_PEN_TABLE && transparency != TRANSPARENCY_PEN_TABLE_RAW
			&& transparency != TRANSPARENCY_BLEND_RAW && transparency != TRANSPARENCY_ALPHAONE
			&& transparency != TRANSPARENCY_ALPHA && transparency != TRANSPARENCY_ALPHARANGE
			&& transparency != TRANSPARENCY_NONE)
	{
		usrintf_showmessage("drawgfxzoom unsupported trans %02x",transparency);
		return;
	}

	if (!alpha_active && (transparency == TRANSPARENCY_ALPHAONE || transparency == TRANSPARENCY_ALPHA || transparency == TRANSPARENCY_ALPHARANGE))
	{
		transparency = TRANSPARENCY_PEN;
		transparent_color &= 0xff;
	}

	if (transparency == TRANSPARENCY_ALPHAONE)
	{
		alphapen = transparent_color >> 8;
		transparent_color &= 0xff;
	}

	if (transparency == TRANSPARENCY_COLOR)
		transparent_color = Machine->pens[transparent_color];


	/*
	scalex and scaley are 16.16 fixed point numbers
	1<<15 : shrink to 50%
	1<<16 : uniform scale
	1<<17 : double to 200%
	*/


	/* KW 991012 -- Added code to force clip to bitmap boundary */
	if(clip)
	{
		myclip.min_x = clip->min_x;
		myclip.max_x = clip->max_x;
		myclip.min_y = clip->min_y;
		myclip.max_y = clip->max_y;

		if (myclip.min_x < 0) myclip.min_x = 0;
		if (myclip.max_x >= dest_bmp->width) myclip.max_x = dest_bmp->width-1;
		if (myclip.min_y < 0) myclip.min_y = 0;
		if (myclip.max_y >= dest_bmp->height) myclip.max_y = dest_bmp->height-1;

		clip=&myclip;
	}


	/* ASG 980209 -- added 16-bit version */
	if (dest_bmp->depth == 8)
	{
		if( gfx && gfx->colortable )
		{
			const pen_t *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			UINT8 *source_base = gfx->gfxdata + (code % gfx->total_elements) * gfx->char_modulo;

			int sprite_screen_height = (scaley*gfx->height+0x8000)>>16;
			int sprite_screen_width = (scalex*gfx->width+0x8000)>>16;

			if (sprite_screen_width && sprite_screen_height)
			{
				/* compute sprite increment per screen pixel */
				int dx = (gfx->width<<16)/sprite_screen_width;
				int dy = (gfx->height<<16)/sprite_screen_height;

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

				if( clip )
				{
					if( sx < clip->min_x)
					{ /* clip left */
						int pixels = clip->min_x-sx;
						sx += pixels;
						x_index_base += pixels*dx;
					}
					if( sy < clip->min_y )
					{ /* clip top */
						int pixels = clip->min_y-sy;
						sy += pixels;
						y_index += pixels*dy;
					}
					/* NS 980211 - fixed incorrect clipping */
					if( ex > clip->max_x+1 )
					{ /* clip right */
						int pixels = ex-clip->max_x-1;
						ex -= pixels;
					}
					if( ey > clip->max_y+1 )
					{ /* clip bottom */
						int pixels = ey-clip->max_y-1;
						ey -= pixels;
					}
				}

				if( ex>sx )
				{ /* skip if inner loop doesn't draw anything */
					int y;

					/* case 0: TRANSPARENCY_NONE */
					if (transparency == TRANSPARENCY_NONE)
					{
						if (pri_buffer)
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[(source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f];
										pri[x] = 31;
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[source[x_index>>16]];
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
						else
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										dest[x] = pal[(source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f];
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										dest[x] = pal[source[x_index>>16]];
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
					}

					/* case 1: TRANSPARENCY_PEN */
					if (transparency == TRANSPARENCY_PEN)
					{
						if (pri_buffer)
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = (source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f;
										if( c != transparent_color )
										{
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = pal[c];
											pri[x] = 31;
										}
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = source[x_index>>16];
										if( c != transparent_color )
										{
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = pal[c];
											pri[x] = 31;
										}
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
						else
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = (source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f;
										if( c != transparent_color ) dest[x] = pal[c];
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT8 *dest = dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = source[x_index>>16];
										if( c != transparent_color ) dest[x] = pal[c];
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
					}

					/* case 1b: TRANSPARENCY_PEN_RAW */
					if (transparency == TRANSPARENCY_PEN_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = color + c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = color + c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 1c: TRANSPARENCY_BLEND_RAW */
					if (transparency == TRANSPARENCY_BLEND_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] |= (color + c);
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] |= (color + c);
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 2: TRANSPARENCY_PENS */
					if (transparency == TRANSPARENCY_PENS)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[c];
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
										dest[x] = pal[c];
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 3: TRANSPARENCY_COLOR */
					else if (transparency == TRANSPARENCY_COLOR)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color ) dest[x] = c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 4: TRANSPARENCY_PEN_TABLE */
					if (transparency == TRANSPARENCY_PEN_TABLE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											if (((1 << (pri[x] & 0x1f)) & pri_mask) == 0)
											{
												if (pri[x] & 0x80)
													dest[x] = palette_shadow_table[pal[c]];
												else
													dest[x] = pal[c];
											}
											pri[x] = (pri[x] & 0x7f) | 31;
											break;
										case DRAWMODE_SHADOW:
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = palette_shadow_table[dest[x]];
											pri[x] |= al;
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = pal[c];
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 4b: TRANSPARENCY_PEN_TABLE_RAW */
					if (transparency == TRANSPARENCY_PEN_TABLE_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											if (((1 << (pri[x] & 0x1f)) & pri_mask) == 0)
											{
												if (pri[x] & 0x80)
													dest[x] = palette_shadow_table[color + c];
												else
													dest[x] = color + c;
											}
											pri[x] = (pri[x] & 0x7f) | 31;
											break;
										case DRAWMODE_SHADOW:
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = palette_shadow_table[dest[x]];
											pri[x] |= al;
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT8 *dest = dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = color + c;
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
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
	}

	/* ASG 980209 -- new 16-bit part */
	else if (dest_bmp->depth == 15 || dest_bmp->depth == 16)
	{
		if( gfx && gfx->colortable )
		{
			const pen_t *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			UINT8 *source_base = gfx->gfxdata + (code % gfx->total_elements) * gfx->char_modulo;

			int sprite_screen_height = (scaley*gfx->height+0x8000)>>16;
			int sprite_screen_width = (scalex*gfx->width+0x8000)>>16;

			if (sprite_screen_width && sprite_screen_height)
			{
				/* compute sprite increment per screen pixel */
				int dx = (gfx->width<<16)/sprite_screen_width;
				int dy = (gfx->height<<16)/sprite_screen_height;

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

				if( clip )
				{
					if( sx < clip->min_x)
					{ /* clip left */
						int pixels = clip->min_x-sx;
						sx += pixels;
						x_index_base += pixels*dx;
					}
					if( sy < clip->min_y )
					{ /* clip top */
						int pixels = clip->min_y-sy;
						sy += pixels;
						y_index += pixels*dy;
					}
					/* NS 980211 - fixed incorrect clipping */
					if( ex > clip->max_x+1 )
					{ /* clip right */
						int pixels = ex-clip->max_x-1;
						ex -= pixels;
					}
					if( ey > clip->max_y+1 )
					{ /* clip bottom */
						int pixels = ey-clip->max_y-1;
						ey -= pixels;
					}
				}

				if( ex>sx )
				{ /* skip if inner loop doesn't draw anything */
					int y;

					/* case 0: TRANSPARENCY_NONE */
					if (transparency == TRANSPARENCY_NONE)
					{
						if (pri_buffer)
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[(source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f];
										pri[x] = 31;
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[source[x_index>>16]];
										pri[x] = 31;
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
						else
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										dest[x] = pal[(source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f];
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										dest[x] = pal[source[x_index>>16]];
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
					}

					/* case 1: TRANSPARENCY_PEN */
					if (transparency == TRANSPARENCY_PEN)
					{
						if (pri_buffer)
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = (source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f;
										if( c != transparent_color )
										{
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = pal[c];
											pri[x] = 31;
										}
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];
									UINT8 *pri = pri_buffer->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = source[x_index>>16];
										if( c != transparent_color )
										{
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = pal[c];
											pri[x] = 31;
										}
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
						else
						{
							if (gfx->flags & GFX_PACKED)
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = (source[x_index>>17] >> ((x_index & 0x10000) >> 14)) & 0x0f;
										if( c != transparent_color ) dest[x] = pal[c];
										x_index += dx;
									}

									y_index += dy;
								}
							}
							else
							{
								for( y=sy; y<ey; y++ )
								{
									UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
									UINT16 *dest = (UINT16 *)dest_bmp->line[y];

									int x, x_index = x_index_base;
									for( x=sx; x<ex; x++ )
									{
										int c = source[x_index>>16];
										if( c != transparent_color ) dest[x] = pal[c];
										x_index += dx;
									}

									y_index += dy;
								}
							}
						}
					}

					/* case 1b: TRANSPARENCY_PEN_RAW */
					if (transparency == TRANSPARENCY_PEN_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = color + c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = color + c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 1c: TRANSPARENCY_BLEND_RAW */
					if (transparency == TRANSPARENCY_BLEND_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] |= color + c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] |= color + c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 2: TRANSPARENCY_PENS */
					if (transparency == TRANSPARENCY_PENS)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[c];
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
										dest[x] = pal[c];
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 3: TRANSPARENCY_COLOR */
					else if (transparency == TRANSPARENCY_COLOR)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color ) dest[x] = c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 4: TRANSPARENCY_PEN_TABLE */
					if (transparency == TRANSPARENCY_PEN_TABLE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											ah = pri[x];
											if (((1 << (ah & 0x1f)) & pri_mask) == 0)
											{
												if (ah & 0x80)
													dest[x] = palette_shadow_table[pal[c]];
												else
													dest[x] = pal[c];
											}
											pri[x] = (ah & 0x7f) | 31;
											break;
										case DRAWMODE_SHADOW:
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = palette_shadow_table[dest[x]];
											pri[x] |= al;
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = pal[c];
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 4b: TRANSPARENCY_PEN_TABLE_RAW */
					if (transparency == TRANSPARENCY_PEN_TABLE_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											ah = pri[x];
											if (((1 << (ah & 0x1f)) & pri_mask) == 0)
											{
												if (ah & 0x80)
													dest[x] = palette_shadow_table[color + c];
												else
													dest[x] = color + c;
											}
											pri[x] = (ah & 0x7f) | 31;
											break;
										case DRAWMODE_SHADOW:
											if (((1 << pri[x]) & pri_mask) == 0)
												dest[x] = palette_shadow_table[dest[x]];
											pri[x] |= al;
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										switch(gfx_drawmode_table[c])
										{
										case DRAWMODE_SOURCE:
											dest[x] = color + c;
											break;
										case DRAWMODE_SHADOW:
											dest[x] = palette_shadow_table[dest[x]];
											break;
										}
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 5: TRANSPARENCY_ALPHAONE */
					if (transparency == TRANSPARENCY_ALPHAONE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
										{
											if( c == alphapen)
												dest[x] = alpha_blend16(dest[x], pal[c]);
											else
												dest[x] = pal[c];
										}
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if( c == alphapen)
											dest[x] = alpha_blend16(dest[x], pal[c]);
										else
											dest[x] = pal[c];
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 6: TRANSPARENCY_ALPHA */
					if (transparency == TRANSPARENCY_ALPHA)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = alpha_blend16(dest[x], pal[c]);
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = alpha_blend16(dest[x], pal[c]);
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* pjp 31/5/02 */
					/* case 7: TRANSPARENCY_ALPHARANGE */
					if (transparency == TRANSPARENCY_ALPHARANGE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
										{
											if( gfx_alpharange_table[c] == 0xff )
												dest[x] = pal[c];
											else
												dest[x] = alpha_blend_r16(dest[x], pal[c], gfx_alpharange_table[c]);
										}
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT16 *dest = (UINT16 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if( gfx_alpharange_table[c] == 0xff )
											dest[x] = pal[c];
										else
											dest[x] = alpha_blend_r16(dest[x], pal[c], gfx_alpharange_table[c]);
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
	}
	else
	{
		if( gfx && gfx->colortable )
		{
			const pen_t *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)]; /* ASG 980209 */
			UINT8 *source_base = gfx->gfxdata + (code % gfx->total_elements) * gfx->char_modulo;

			int sprite_screen_height = (scaley*gfx->height+0x8000)>>16;
			int sprite_screen_width = (scalex*gfx->width+0x8000)>>16;

			if (sprite_screen_width && sprite_screen_height)
			{
				/* compute sprite increment per screen pixel */
				int dx = (gfx->width<<16)/sprite_screen_width;
				int dy = (gfx->height<<16)/sprite_screen_height;

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

				if( clip )
				{
					if( sx < clip->min_x)
					{ /* clip left */
						int pixels = clip->min_x-sx;
						sx += pixels;
						x_index_base += pixels*dx;
					}
					if( sy < clip->min_y )
					{ /* clip top */
						int pixels = clip->min_y-sy;
						sy += pixels;
						y_index += pixels*dy;
					}
					/* NS 980211 - fixed incorrect clipping */
					if( ex > clip->max_x+1 )
					{ /* clip right */
						int pixels = ex-clip->max_x-1;
						ex -= pixels;
					}
					if( ey > clip->max_y+1 )
					{ /* clip bottom */
						int pixels = ey-clip->max_y-1;
						ey -= pixels;
					}
				}

				if( ex>sx )
				{ /* skip if inner loop doesn't draw anything */
					int y;

					/* case 0: TRANSPARENCY_NONE */
					if (transparency == TRANSPARENCY_NONE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									if (((1 << pri[x]) & pri_mask) == 0)
										dest[x] = pal[source[x_index>>16]];
									pri[x] = 31;
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									dest[x] = pal[source[x_index>>16]];
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 1: TRANSPARENCY_PEN */
					if (transparency == TRANSPARENCY_PEN)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[c];
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = pal[c];
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 1b: TRANSPARENCY_PEN_RAW */
					if (transparency == TRANSPARENCY_PEN_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = color + c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = color + c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 1c: TRANSPARENCY_BLEND_RAW */
					if (transparency == TRANSPARENCY_BLEND_RAW)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] |= color + c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] |= color + c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 2: TRANSPARENCY_PENS */
					if (transparency == TRANSPARENCY_PENS)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = pal[c];
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if (((1 << c) & transparent_color) == 0)
										dest[x] = pal[c];
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 3: TRANSPARENCY_COLOR */
					else if (transparency == TRANSPARENCY_COLOR)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = c;
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = pal[source[x_index>>16]];
									if( c != transparent_color ) dest[x] = c;
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 4: TRANSPARENCY_PEN_TABLE */
					if (transparency == TRANSPARENCY_PEN_TABLE)
					{
						UINT8 *source, *pri;
						UINT32 *dest;
						int c, x, x_index;

						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								source = source_base + (y_index>>16) * gfx->line_modulo;
								y_index += dy;
								dest = (UINT32 *)dest_bmp->line[y];
								pri = pri_buffer->line[y];
								x_index = x_index_base;

								for( x=sx; x<ex; x++ )
								{
									int ebx = x_index;
									x_index += dx;
									ebx >>= 16;
									al = pri[x];
									c = source[ebx];
									ah = al;
									al &= 0x1f;

									if (gfx_drawmode_table[c] == DRAWMODE_NONE) continue;

									if (!(1<<al & pri_mask))
									{
										if (gfx_drawmode_table[c] == DRAWMODE_SOURCE)
										{
											ah &= 0x7f;
											ebx = pal[c];
											ah |= 0x1f;
											dest[x] = ebx;
											pri[x] = ah;
										}
										else if (!(ah & 0x80))
										{
											ebx = SHADOW32(dest[x]);
											pri[x] |= 0x80;
											dest[x] = ebx;
										}
									}
								}
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								source = source_base + (y_index>>16) * gfx->line_modulo;
								y_index += dy;
								dest = (UINT32 *)dest_bmp->line[y];
								x_index = x_index_base;

								for( x=sx; x<ex; x++ )
								{
									c = source[x_index>>16];
									x_index += dx;

									if (gfx_drawmode_table[c] == DRAWMODE_NONE) continue;
									if (gfx_drawmode_table[c] == DRAWMODE_SOURCE)
										dest[x] = pal[c];
									else
										dest[x] = SHADOW32(dest[x]);
								}
							}
						}
					}

					/* case 4b: TRANSPARENCY_PEN_TABLE_RAW */
					if (transparency == TRANSPARENCY_PEN_TABLE_RAW)
					{
						UINT8 *source, *pri;
						UINT32 *dest;
						int c, x, x_index;

						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								source = source_base + (y_index>>16) * gfx->line_modulo;
								y_index += dy;
								dest = (UINT32 *)dest_bmp->line[y];
								pri = pri_buffer->line[y];
								x_index = x_index_base;

								for( x=sx; x<ex; x++ )
								{
									int ebx = x_index;
									x_index += dx;
									ebx >>= 16;
									al = pri[x];
									c = source[ebx];
									ah = al;
									al &= 0x1f;

									if (gfx_drawmode_table[c] == DRAWMODE_NONE) continue;

									if (!(1<<al & pri_mask))
									{
										if (gfx_drawmode_table[c] == DRAWMODE_SOURCE)
										{
											ah &= 0x7f;
											ebx = color + c;
											ah |= 0x1f;
											dest[x] = ebx;
											pri[x] = ah;
										}
										else if (!(ah & 0x80))
										{
											ebx = SHADOW32(dest[x]);
											pri[x] |= 0x80;
											dest[x] = ebx;
										}
									}
								}
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								source = source_base + (y_index>>16) * gfx->line_modulo;
								y_index += dy;
								dest = (UINT32 *)dest_bmp->line[y];
								x_index = x_index_base;

								for( x=sx; x<ex; x++ )
								{
									c = source[x_index>>16];
									x_index += dx;

									if (gfx_drawmode_table[c] == DRAWMODE_NONE) continue;
									if (gfx_drawmode_table[c] == DRAWMODE_SOURCE)
										dest[x] = color + c;
									else
										dest[x] = SHADOW32(dest[x]);
								}
							}
						}
					}


					/* case 5: TRANSPARENCY_ALPHAONE */
					if (transparency == TRANSPARENCY_ALPHAONE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
										{
											if( c == alphapen)
												dest[x] = alpha_blend32(dest[x], pal[c]);
											else
												dest[x] = pal[c];
										}
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if( c == alphapen)
											dest[x] = alpha_blend32(dest[x], pal[c]);
										else
											dest[x] = pal[c];
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* case 6: TRANSPARENCY_ALPHA */
					if (transparency == TRANSPARENCY_ALPHA)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
											dest[x] = alpha_blend32(dest[x], pal[c]);
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color ) dest[x] = alpha_blend32(dest[x], pal[c]);
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}

					/* pjp 31/5/02 */
					/* case 7: TRANSPARENCY_ALPHARANGE */
					if (transparency == TRANSPARENCY_ALPHARANGE)
					{
						if (pri_buffer)
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *pri = pri_buffer->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if (((1 << pri[x]) & pri_mask) == 0)
										{
											if( gfx_alpharange_table[c] == 0xff )
												dest[x] = pal[c];
											else
												dest[x] = alpha_blend_r32(dest[x], pal[c], gfx_alpharange_table[c]);
										}
										pri[x] = 31;
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
						else
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = source_base + (y_index>>16) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c != transparent_color )
									{
										if( gfx_alpharange_table[c] == 0xff )
											dest[x] = pal[c];
										else
											dest[x] = alpha_blend_r32(dest[x], pal[c], gfx_alpharange_table[c]);
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
	}
}

void drawgfxzoom( struct mame_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex, int scaley)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfxzoom(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,
			clip,transparency,transparent_color,scalex,scaley,NULL,0);
	profiler_mark(PROFILER_END);
}

void pdrawgfxzoom( struct mame_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex, int scaley,
		UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfxzoom(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,
			clip,transparency,transparent_color,scalex,scaley,priority_bitmap,priority_mask | (1<<31));
	profiler_mark(PROFILER_END);
}

void mdrawgfxzoom( struct mame_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,int scalex, int scaley,
		UINT32 priority_mask)
{
	profiler_mark(PROFILER_DRAWGFX);
	common_drawgfxzoom(dest_bmp,gfx,code,color,flipx,flipy,sx,sy,
			clip,transparency,transparent_color,scalex,scaley,priority_bitmap,priority_mask);
	profiler_mark(PROFILER_END);
}

void plot_pixel2(struct mame_bitmap *bitmap1,struct mame_bitmap *bitmap2,int x,int y,pen_t pen)
{
	plot_pixel(bitmap1, x, y, pen);
	plot_pixel(bitmap2, x, y, pen);
}

static void pp_8(struct mame_bitmap *b,int x,int y,pen_t p)  { ((UINT8 *)b->line[y])[x] = p; }
static void pp_16(struct mame_bitmap *b,int x,int y,pen_t p)  { ((UINT16 *)b->line[y])[x] = p; }
static void pp_32(struct mame_bitmap *b,int x,int y,pen_t p)  { ((UINT32 *)b->line[y])[x] = p; }

static pen_t rp_8(struct mame_bitmap *b,int x,int y)  { return ((UINT8 *)b->line[y])[x]; }
static pen_t rp_16(struct mame_bitmap *b,int x,int y)  { return ((UINT16 *)b->line[y])[x]; }
static pen_t rp_32(struct mame_bitmap *b,int x,int y)  { return ((UINT32 *)b->line[y])[x]; }

static void pb_8(struct mame_bitmap *b,int x,int y,int w,int h,pen_t p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT8 *)b->line[y])[x] = p; x++; } y++; } }
static void pb_16(struct mame_bitmap *b,int x,int y,int w,int h,pen_t p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT16 *)b->line[y])[x] = p; x++; } y++; } }
static void pb_32(struct mame_bitmap *b,int x,int y,int w,int h,pen_t p)  { int t=x; while(h-->0){ int c=w; x=t; while(c-->0){ ((UINT32 *)b->line[y])[x] = p; x++; } y++; } }


void set_pixel_functions(struct mame_bitmap *bitmap)
{
	if (bitmap->depth == 8)
	{
		bitmap->read = rp_8;
		bitmap->plot = pp_8;
		bitmap->plot_box = pb_8;
	}
	else if(bitmap->depth == 15 || bitmap->depth == 16)
	{
		bitmap->read = rp_16;
		bitmap->plot = pp_16;
		bitmap->plot_box = pb_16;
	}
	else
	{
		bitmap->read = rp_32;
		bitmap->plot = pp_32;
		bitmap->plot_box = pb_32;
	}

	/* while we're here, fill in the raw drawing mode table as well */
	is_raw[TRANSPARENCY_NONE_RAW]      = 1;
	is_raw[TRANSPARENCY_PEN_RAW]       = 1;
	is_raw[TRANSPARENCY_PENS_RAW]      = 1;
	is_raw[TRANSPARENCY_PEN_TABLE_RAW] = 1;
	is_raw[TRANSPARENCY_BLEND_RAW]     = 1;
}


static INLINE void plotclip(struct mame_bitmap *bitmap,int x,int y,int pen,const struct rectangle *clip)
{
	if (x >= clip->min_x && x <= clip->max_x && y >= clip->min_y && y <= clip->max_y)
		plot_pixel(bitmap,x,y,pen);
}

void draw_crosshair(int player_number, struct mame_bitmap *bitmap,int x,int y,const struct rectangle *clip)
{
	unsigned long color,black,white;
	int i;
	static int inactive_xy [MAX_PLAYER_COUNT][3];

	if (!options.crosshair_enable || cpu_pause_state)
	{
		/* Update frame count */
		inactive_xy[player_number-1][2] = cpu_getcurrentframe();
		return;
	}

	/* Catch if frame count was reset */
	if (inactive_xy[player_number-1][2] > cpu_getcurrentframe())
		inactive_xy[player_number-1][2] = cpu_getcurrentframe();

	/* Hide crosshairs for inactive players */
	if(inactive_xy[player_number-1][0] == x && inactive_xy[player_number-1][1] == y)
	{
		if(inactive_xy[player_number-1][2] + (Machine->drv->frames_per_second * 15) < cpu_getcurrentframe())
			return;
	}
	else
	{
		inactive_xy[player_number-1][0] = x;
		inactive_xy[player_number-1][1] = y;
		inactive_xy[player_number-1][2] = cpu_getcurrentframe();
	}

	/* Color crosshairs */
	black = Machine->uifont->colortable[0];
	white = Machine->uifont->colortable[1];
	color = white; /* default */

	/* Draw crosshair - simple */
	for (i = 1;i < 6;i++)
	{
		plotclip(bitmap,x+i,y,color,clip);
		plotclip(bitmap,x-i,y,color,clip);
		plotclip(bitmap,x,y+i,color,clip);
		plotclip(bitmap,x,y-i,color,clip);
	}

	/* Draw crosshair - enhanced, adds to simple design above */
	if(options.crosshair_appearance == 1)
	{
		/* Outter long lines */
		for (i = -3;i < 4;i++)
		{
			plotclip(bitmap,x+7,y+i,color,clip);
			plotclip(bitmap,x-7,y+i,color,clip);
			plotclip(bitmap,x+i,y+7,color,clip);
			plotclip(bitmap,x+i,y-7,color,clip);
		}

		/* Inner short lines */
		for (i = -1;i < 2;i++)
		{
			plotclip(bitmap,x+6,y+i,color,clip);
			plotclip(bitmap,x-6,y+i,color,clip);
			plotclip(bitmap,x+i,y+6,color,clip);
			plotclip(bitmap,x+i,y-6,color,clip);
		}

		/* 45 degrees */
		for (i = 0;i < 3;i++)
		{
			plotclip(bitmap,x-6+i,y+4+i,color,clip);
			plotclip(bitmap,x-6+i,y-4-i,color,clip);
			plotclip(bitmap,x+6-i,y+4+i,color,clip);
			plotclip(bitmap,x+6-i,y-4-i,color,clip);
		}
	}
}


#else /* DECLARE */

/* -------------------- included inline section --------------------- */

/* this is #included to generate 8-bit and 16-bit versions */

#define ADJUST_8													\
	int ydir;														\
	if (flipy)														\
	{																\
		INCREMENT_DST(VMODULO * (dstheight-1))						\
		srcdata += (srcheight - dstheight - topskip) * srcmodulo;	\
		ydir = -1;													\
	}																\
	else															\
	{																\
		srcdata += topskip * srcmodulo;								\
		ydir = 1;													\
	}																\
	if (flipx)														\
	{																\
		INCREMENT_DST(HMODULO * (dstwidth-1))						\
		srcdata += (srcwidth - dstwidth - leftskip);				\
	}																\
	else															\
		srcdata += leftskip;										\
	srcmodulo -= dstwidth;


#define ADJUST_4													\
	int ydir;														\
	if (flipy)														\
	{																\
		INCREMENT_DST(VMODULO * (dstheight-1))						\
		srcdata += (srcheight - dstheight - topskip) * srcmodulo;	\
		ydir = -1;													\
	}																\
	else															\
	{																\
		srcdata += topskip * srcmodulo;								\
		ydir = 1;													\
	}																\
	if (flipx)														\
	{																\
		INCREMENT_DST(HMODULO * (dstwidth-1))						\
		srcdata += (srcwidth - dstwidth - leftskip)/2;				\
		leftskip = (srcwidth - dstwidth - leftskip) & 1;			\
	}																\
	else															\
	{																\
		srcdata += leftskip/2;										\
		leftskip &= 1;												\
	}																\
	srcmodulo -= (dstwidth+leftskip)/2;



DECLARE_SWAP_RAW_PRI(blockmove_8toN_opaque,(COMMON_ARGS,
		COLOR_ARG),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (dstdata >= end + 8*HMODULO)
			{
				INCREMENT_DST(-8*HMODULO)
				SETPIXELCOLOR(8*HMODULO,LOOKUP(srcdata[0]))
				SETPIXELCOLOR(7*HMODULO,LOOKUP(srcdata[1]))
				SETPIXELCOLOR(6*HMODULO,LOOKUP(srcdata[2]))
				SETPIXELCOLOR(5*HMODULO,LOOKUP(srcdata[3]))
				SETPIXELCOLOR(4*HMODULO,LOOKUP(srcdata[4]))
				SETPIXELCOLOR(3*HMODULO,LOOKUP(srcdata[5]))
				SETPIXELCOLOR(2*HMODULO,LOOKUP(srcdata[6]))
				SETPIXELCOLOR(1*HMODULO,LOOKUP(srcdata[7]))
				srcdata += 8;
			}
			while (dstdata > end)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata))
				srcdata++;
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (dstdata <= end - 8*HMODULO)
			{
				SETPIXELCOLOR(0*HMODULO,LOOKUP(srcdata[0]))
				SETPIXELCOLOR(1*HMODULO,LOOKUP(srcdata[1]))
				SETPIXELCOLOR(2*HMODULO,LOOKUP(srcdata[2]))
				SETPIXELCOLOR(3*HMODULO,LOOKUP(srcdata[3]))
				SETPIXELCOLOR(4*HMODULO,LOOKUP(srcdata[4]))
				SETPIXELCOLOR(5*HMODULO,LOOKUP(srcdata[5]))
				SETPIXELCOLOR(6*HMODULO,LOOKUP(srcdata[6]))
				SETPIXELCOLOR(7*HMODULO,LOOKUP(srcdata[7]))
				srcdata += 8;
				INCREMENT_DST(8*HMODULO)
			}
			while (dstdata < end)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata))
				srcdata++;
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_4toN_opaque,(COMMON_ARGS,
		COLOR_ARG),
{
	ADJUST_4

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			if (leftskip)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata>>4))
				srcdata++;
				INCREMENT_DST(-HMODULO)
			}
			while (dstdata >= end + 8*HMODULO)
			{
				INCREMENT_DST(-8*HMODULO)
				SETPIXELCOLOR(8*HMODULO,LOOKUP(srcdata[0]&0x0f))
				SETPIXELCOLOR(7*HMODULO,LOOKUP(srcdata[0]>>4))
				SETPIXELCOLOR(6*HMODULO,LOOKUP(srcdata[1]&0x0f))
				SETPIXELCOLOR(5*HMODULO,LOOKUP(srcdata[1]>>4))
				SETPIXELCOLOR(4*HMODULO,LOOKUP(srcdata[2]&0x0f))
				SETPIXELCOLOR(3*HMODULO,LOOKUP(srcdata[2]>>4))
				SETPIXELCOLOR(2*HMODULO,LOOKUP(srcdata[3]&0x0f))
				SETPIXELCOLOR(1*HMODULO,LOOKUP(srcdata[3]>>4))
				srcdata += 4;
			}
			while (dstdata > end)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata&0x0f))
				INCREMENT_DST(-HMODULO)
				if (dstdata > end)
				{
					SETPIXELCOLOR(0,LOOKUP(*srcdata>>4))
					srcdata++;
					INCREMENT_DST(-HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			if (leftskip)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata>>4))
				srcdata++;
				INCREMENT_DST(HMODULO)
			}
			while (dstdata <= end - 8*HMODULO)
			{
				SETPIXELCOLOR(0*HMODULO,LOOKUP(srcdata[0]&0x0f))
				SETPIXELCOLOR(1*HMODULO,LOOKUP(srcdata[0]>>4))
				SETPIXELCOLOR(2*HMODULO,LOOKUP(srcdata[1]&0x0f))
				SETPIXELCOLOR(3*HMODULO,LOOKUP(srcdata[1]>>4))
				SETPIXELCOLOR(4*HMODULO,LOOKUP(srcdata[2]&0x0f))
				SETPIXELCOLOR(5*HMODULO,LOOKUP(srcdata[2]>>4))
				SETPIXELCOLOR(6*HMODULO,LOOKUP(srcdata[3]&0x0f))
				SETPIXELCOLOR(7*HMODULO,LOOKUP(srcdata[3]>>4))
				srcdata += 4;
				INCREMENT_DST(8*HMODULO)
			}
			while (dstdata < end)
			{
				SETPIXELCOLOR(0,LOOKUP(*srcdata&0x0f))
				INCREMENT_DST(HMODULO)
				if (dstdata < end)
				{
					SETPIXELCOLOR(0,LOOKUP(*srcdata>>4))
					srcdata++;
					INCREMENT_DST(HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_transpen,(COMMON_ARGS,
		COLOR_ARG,int transpen),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata > end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata >= end + 4*HMODULO)
			{
				UINT32 col4;

				INCREMENT_DST(-4*HMODULO)
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(4*HMODULO,LOOKUP((col4>>SHIFT0) & 0xff))
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(3*HMODULO,LOOKUP((col4>>SHIFT1) & 0xff))
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,LOOKUP((col4>>SHIFT2) & 0xff))
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(1*HMODULO,LOOKUP((col4>>SHIFT3) & 0xff))
				}
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO);
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata < end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata <= end - 4*HMODULO)
			{
				UINT32 col4;

				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(0*HMODULO,LOOKUP((col4>>SHIFT0) & 0xff))
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(1*HMODULO,LOOKUP((col4>>SHIFT1) & 0xff))
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,LOOKUP((col4>>SHIFT2) & 0xff))
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(3*HMODULO,LOOKUP((col4>>SHIFT3) & 0xff))
				}
				INCREMENT_DST(4*HMODULO)
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO);
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_4toN_transpen,(COMMON_ARGS,
		COLOR_ARG,int transpen),
{
	ADJUST_4

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			int col;

			end = dstdata - dstwidth*HMODULO;
			if (leftskip)
			{
				col = *(srcdata++)>>4;
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}
			while (dstdata > end)
			{
				col = *(srcdata)&0x0f;
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
				if (dstdata > end)
				{
					col = *(srcdata++)>>4;
					if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
					INCREMENT_DST(-HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			int col;

			end = dstdata + dstwidth*HMODULO;
			if (leftskip)
			{
				col = *(srcdata++)>>4;
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}
			while (dstdata < end)
			{
				col = *(srcdata)&0x0f;
				if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
				if (dstdata < end)
				{
					col = *(srcdata++)>>4;
					if (col != transpen) SETPIXELCOLOR(0,LOOKUP(col))
					INCREMENT_DST(HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_transblend,(COMMON_ARGS,
		COLOR_ARG,int transpen),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata > end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,*dstdata | LOOKUP(col))
				INCREMENT_DST(-HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata >= end + 4*HMODULO)
			{
				UINT32 col4;

				INCREMENT_DST(-4*HMODULO);
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(4*HMODULO,dstdata[4*HMODULO] | LOOKUP((col4>>SHIFT0) & 0xff))
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(3*HMODULO,dstdata[3*HMODULO] | LOOKUP((col4>>SHIFT1) & 0xff))
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,dstdata[2*HMODULO] | LOOKUP((col4>>SHIFT2) & 0xff))
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(1*HMODULO,dstdata[1*HMODULO] | LOOKUP((col4>>SHIFT3) & 0xff))
				}
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,*dstdata | LOOKUP(col))
				INCREMENT_DST(-HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO);
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata < end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,*dstdata | LOOKUP(col))
				INCREMENT_DST(HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata <= end - 4*HMODULO)
			{
				UINT32 col4;

				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(0*HMODULO,dstdata[0*HMODULO] | LOOKUP((col4>>SHIFT0) & 0xff))
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(1*HMODULO,dstdata[1*HMODULO] | LOOKUP((col4>>SHIFT1) & 0xff))
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,dstdata[2*HMODULO] | LOOKUP((col4>>SHIFT2) & 0xff))
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(3*HMODULO,dstdata[3*HMODULO] | LOOKUP((col4>>SHIFT3) & 0xff))
				}
				INCREMENT_DST(4*HMODULO);
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,*dstdata | LOOKUP(col))
				INCREMENT_DST(HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO);
			dstheight--;
		}
	}
})


#define PEN_IS_OPAQUE ((1<<col)&transmask) == 0

DECLARE_SWAP_RAW_PRI(blockmove_8toN_transmask,(COMMON_ARGS,
		COLOR_ARG,int transmask),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;
		UINT32 *sd4;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata > end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata >= end + 4*HMODULO)
			{
				int col;
				UINT32 col4;

				INCREMENT_DST(-4*HMODULO)
				col4 = *(sd4++);
				col = (col4 >> SHIFT0) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(4*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT1) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(3*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT2) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(2*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT3) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(1*HMODULO,LOOKUP(col))
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;
		UINT32 *sd4;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata < end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata <= end - 4*HMODULO)
			{
				int col;
				UINT32 col4;

				col4 = *(sd4++);
				col = (col4 >> SHIFT0) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(0*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT1) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(1*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT2) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(2*HMODULO,LOOKUP(col))
				col = (col4 >> SHIFT3) & 0xff;
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(3*HMODULO,LOOKUP(col))
				INCREMENT_DST(4*HMODULO)
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (PEN_IS_OPAQUE) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_transcolor,(COMMON_ARGS,
		COLOR_ARG,const UINT16 *colortable,int transcolor),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (dstdata > end)
			{
				if (colortable[*srcdata] != transcolor) SETPIXELCOLOR(0,LOOKUP(*srcdata))
				srcdata++;
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (dstdata < end)
			{
				if (colortable[*srcdata] != transcolor) SETPIXELCOLOR(0,LOOKUP(*srcdata))
				srcdata++;
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_4toN_transcolor,(COMMON_ARGS,
		COLOR_ARG,const UINT16 *colortable,int transcolor),
{
	ADJUST_4

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			int col;

			end = dstdata - dstwidth*HMODULO;
			if (leftskip)
			{
				col = *(srcdata++)>>4;
				if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
			}
			while (dstdata > end)
			{
				col = *(srcdata)&0x0f;
				if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(-HMODULO)
				if (dstdata > end)
				{
					col = *(srcdata++)>>4;
					if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
					INCREMENT_DST(-HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			int col;

			end = dstdata + dstwidth*HMODULO;
			if (leftskip)
			{
				col = *(srcdata++)>>4;
				if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
			}
			while (dstdata < end)
			{
				col = *(srcdata)&0x0f;
				if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
				INCREMENT_DST(HMODULO)
				if (dstdata < end)
				{
					col = *(srcdata++)>>4;
					if (colortable[col] != transcolor) SETPIXELCOLOR(0,LOOKUP(col))
					INCREMENT_DST(HMODULO)
				}
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})

#if DEPTH == 32
DECLARE_SWAP_RAW_PRI(blockmove_8toN_pen_table,(COMMON_ARGS,
		COLOR_ARG,int transcolor),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transcolor)
				{
					switch(gfx_drawmode_table[col])
					{
					case DRAWMODE_SOURCE:
						SETPIXELCOLOR(0,LOOKUP(col))
						break;
					case DRAWMODE_SHADOW:
						afterdrawmask = 0;
						SETPIXELCOLOR(0,*dstdata)
						afterdrawmask = 31;
						break;
					}
				}
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transcolor)
				{
					switch(gfx_drawmode_table[col])
					{
					case DRAWMODE_SOURCE:
						SETPIXELCOLOR(0,LOOKUP(col))
						break;
					case DRAWMODE_SHADOW:
						afterdrawmask = 0;
						SETPIXELCOLOR(0,*dstdata)
						afterdrawmask = 31;
						break;
					}
				}
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})
#else
DECLARE_SWAP_RAW_PRI(blockmove_8toN_pen_table,(COMMON_ARGS,
		COLOR_ARG,int transcolor),
{
	int eax = (pdrawgfx_shadow_lowpri) ? 0 : 0x80;

	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transcolor)
				{
					switch(gfx_drawmode_table[col])
					{
					case DRAWMODE_SOURCE:
						SETPIXELCOLOR(0,LOOKUP(col))
						break;
					case DRAWMODE_SHADOW:
						afterdrawmask = eax;
						SETPIXELCOLOR(0,palette_shadow_table[*dstdata])
						afterdrawmask = 31;
						break;
					}
				}
				INCREMENT_DST(-HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO)
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transcolor)
				{
					switch(gfx_drawmode_table[col])
					{
					case DRAWMODE_SOURCE:
						SETPIXELCOLOR(0,LOOKUP(col))
						break;
					case DRAWMODE_SHADOW:
						afterdrawmask = eax;
						SETPIXELCOLOR(0,palette_shadow_table[*dstdata])
						afterdrawmask = 31;
						break;
					}
				}
				INCREMENT_DST(HMODULO)
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO)
			dstheight--;
		}
	}
})
#endif

#if DEPTH >= 16
DECLARE_SWAP_RAW_PRI(blockmove_8toN_alphaone,(COMMON_ARGS,
		COLOR_ARG,int transpen, int alphapen),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;
		UINT32 alphacolor = LOOKUP(alphapen);

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata > end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (col == alphapen)
						SETPIXELCOLOR(0,alpha_blend(*dstdata,alphacolor))
					else
						SETPIXELCOLOR(0,LOOKUP(col))
				}
				INCREMENT_DST(-HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata >= end + 4*HMODULO)
			{
				UINT32 col4;

				INCREMENT_DST(-4*HMODULO);
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0))
					{
						if (((col4>>SHIFT0) & 0xff) == alphapen)
							SETPIXELCOLOR(4*HMODULO,alpha_blend(dstdata[4*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(4*HMODULO,LOOKUP((col4>>SHIFT0) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT1))
					{
						if (((col4>>SHIFT1) & 0xff) == alphapen)
							SETPIXELCOLOR(3*HMODULO,alpha_blend(dstdata[3*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(3*HMODULO,LOOKUP((col4>>SHIFT1) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT2))
					{
						if (((col4>>SHIFT2) & 0xff) == alphapen)
							SETPIXELCOLOR(2*HMODULO,alpha_blend(dstdata[2*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(2*HMODULO,LOOKUP((col4>>SHIFT2) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT3))
					{
						if (((col4>>SHIFT3) & 0xff) == alphapen)
							SETPIXELCOLOR(1*HMODULO,alpha_blend(dstdata[1*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(1*HMODULO,LOOKUP((col4>>SHIFT3) & 0xff))
					}
				}
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (col == alphapen)
						SETPIXELCOLOR(0,alpha_blend(*dstdata, alphacolor))
					else
						SETPIXELCOLOR(0,LOOKUP(col))
				}
				INCREMENT_DST(-HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO);
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;
		UINT32 alphacolor = LOOKUP(alphapen);

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata < end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (col == alphapen)
						SETPIXELCOLOR(0,alpha_blend(*dstdata, alphacolor))
					else
						SETPIXELCOLOR(0,LOOKUP(col))
				}
				INCREMENT_DST(HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata <= end - 4*HMODULO)
			{
				UINT32 col4;

				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0))
					{
						if (((col4>>SHIFT0) & 0xff) == alphapen)
							SETPIXELCOLOR(0*HMODULO,alpha_blend(dstdata[0*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(0*HMODULO,LOOKUP((col4>>SHIFT0) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT1))
					{
						if (((col4>>SHIFT1) & 0xff) == alphapen)
							SETPIXELCOLOR(1*HMODULO,alpha_blend(dstdata[1*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(1*HMODULO,LOOKUP((col4>>SHIFT1) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT2))
					{
						if (((col4>>SHIFT2) & 0xff) == alphapen)
							SETPIXELCOLOR(2*HMODULO,alpha_blend(dstdata[2*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(2*HMODULO,LOOKUP((col4>>SHIFT2) & 0xff))
					}
					if (xod4 & (0xff<<SHIFT3))
					{
						if (((col4>>SHIFT3) & 0xff) == alphapen)
							SETPIXELCOLOR(3*HMODULO,alpha_blend(dstdata[3*HMODULO], alphacolor))
						else
							SETPIXELCOLOR(3*HMODULO,LOOKUP((col4>>SHIFT3) & 0xff))
					}
				}
				INCREMENT_DST(4*HMODULO);
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (col == alphapen)
						SETPIXELCOLOR(0,alpha_blend(*dstdata, alphacolor))
					else
						SETPIXELCOLOR(0,LOOKUP(col))
				}
				INCREMENT_DST(HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO);
			dstheight--;
		}
	}
})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_alpha,(COMMON_ARGS,
		COLOR_ARG,int transpen),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata > end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,alpha_blend(*dstdata, LOOKUP(col)));
				INCREMENT_DST(-HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata >= end + 4*HMODULO)
			{
				UINT32 col4;

				INCREMENT_DST(-4*HMODULO);
				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(4*HMODULO,alpha_blend(dstdata[4*HMODULO], LOOKUP((col4>>SHIFT0) & 0xff)));
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(3*HMODULO,alpha_blend(dstdata[3*HMODULO], LOOKUP((col4>>SHIFT1) & 0xff)));
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,alpha_blend(dstdata[2*HMODULO], LOOKUP((col4>>SHIFT2) & 0xff)));
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(1*HMODULO,alpha_blend(dstdata[1*HMODULO], LOOKUP((col4>>SHIFT3) & 0xff)));
				}
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata > end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,alpha_blend(*dstdata, LOOKUP(col)));
				INCREMENT_DST(-HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO);
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;
		int trans4;
		UINT32 *sd4;

		trans4 = transpen * 0x01010101;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (((long)srcdata & 3) && dstdata < end)	/* longword align */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,alpha_blend(*dstdata, LOOKUP(col)));
				INCREMENT_DST(HMODULO);
			}
			sd4 = (UINT32 *)srcdata;
			while (dstdata <= end - 4*HMODULO)
			{
				UINT32 col4;

				if ((col4 = *(sd4++)) != trans4)
				{
					UINT32 xod4;

					xod4 = col4 ^ trans4;
					if (xod4 & (0xff<<SHIFT0)) SETPIXELCOLOR(0*HMODULO,alpha_blend(dstdata[0*HMODULO], LOOKUP((col4>>SHIFT0) & 0xff)));
					if (xod4 & (0xff<<SHIFT1)) SETPIXELCOLOR(1*HMODULO,alpha_blend(dstdata[1*HMODULO], LOOKUP((col4>>SHIFT1) & 0xff)));
					if (xod4 & (0xff<<SHIFT2)) SETPIXELCOLOR(2*HMODULO,alpha_blend(dstdata[2*HMODULO], LOOKUP((col4>>SHIFT2) & 0xff)));
					if (xod4 & (0xff<<SHIFT3)) SETPIXELCOLOR(3*HMODULO,alpha_blend(dstdata[3*HMODULO], LOOKUP((col4>>SHIFT3) & 0xff)));
				}
				INCREMENT_DST(4*HMODULO);
			}
			srcdata = (UINT8 *)sd4;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen) SETPIXELCOLOR(0,alpha_blend(*dstdata, LOOKUP(col)));
				INCREMENT_DST(HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO);
			dstheight--;
		}
	}
})

/* pjp 02/06/02 */
DECLARE_SWAP_RAW_PRI(blockmove_8toN_alpharange,(COMMON_ARGS,
		COLOR_ARG,int transpen),
{
	ADJUST_8

	if (flipx)
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata - dstwidth*HMODULO;
			while (dstdata > end) /* Note that I'm missing the optimisations present in the other alpha functions */
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (gfx_alpharange_table[col] == 0xff)
						SETPIXELCOLOR(0,LOOKUP(col))
					else
						SETPIXELCOLOR(0,alpha_blend_r(*dstdata,LOOKUP(col),gfx_alpharange_table[col]))
				}
				INCREMENT_DST(-HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO + dstwidth*HMODULO);
			dstheight--;
		}
	}
	else
	{
		DATA_TYPE *end;

		while (dstheight)
		{
			end = dstdata + dstwidth*HMODULO;
			while (dstdata < end)
			{
				int col;

				col = *(srcdata++);
				if (col != transpen)
				{
					if (gfx_alpharange_table[col] == 0xff)
						SETPIXELCOLOR(0,LOOKUP(col))
					else
						SETPIXELCOLOR(0,alpha_blend_r(*dstdata,LOOKUP(col),gfx_alpharange_table[col]))
				}
				INCREMENT_DST(HMODULO);
			}

			srcdata += srcmodulo;
			INCREMENT_DST(ydir*VMODULO - dstwidth*HMODULO);
			dstheight--;
		}
	}
})

#else

DECLARE_SWAP_RAW_PRI(blockmove_8toN_alphaone,(COMMON_ARGS,
		COLOR_ARG,int transpen, int alphapen),{})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_alpha,(COMMON_ARGS,
		COLOR_ARG,int transpen),{})

DECLARE_SWAP_RAW_PRI(blockmove_8toN_alpharange,(COMMON_ARGS,
		COLOR_ARG,int transpen),{})

#endif

DECLARE(blockmove_NtoN_opaque_noremap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo),
{
	while (srcheight)
	{
		memcpy(dstdata,srcdata,srcwidth * sizeof(DATA_TYPE));
		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_noremap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = srcdata[8];
			dstdata[1] = srcdata[7];
			dstdata[2] = srcdata[6];
			dstdata[3] = srcdata[5];
			dstdata[4] = srcdata[4];
			dstdata[5] = srcdata[3];
			dstdata[6] = srcdata[2];
			dstdata[7] = srcdata[1];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = *(srcdata--);

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_remap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const pen_t *paldata),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = paldata[srcdata[0]];
			dstdata[1] = paldata[srcdata[1]];
			dstdata[2] = paldata[srcdata[2]];
			dstdata[3] = paldata[srcdata[3]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[5]];
			dstdata[6] = paldata[srcdata[6]];
			dstdata[7] = paldata[srcdata[7]];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata++)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_opaque_remap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const pen_t *paldata),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = paldata[srcdata[8]];
			dstdata[1] = paldata[srcdata[7]];
			dstdata[2] = paldata[srcdata[6]];
			dstdata[3] = paldata[srcdata[5]];
			dstdata[4] = paldata[srcdata[4]];
			dstdata[5] = paldata[srcdata[3]];
			dstdata[6] = paldata[srcdata[2]];
			dstdata[7] = paldata[srcdata[1]];
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) = paldata[*(srcdata--)];

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})


DECLARE(blockmove_NtoN_blend_noremap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int srcshift),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] |= srcdata[0] << srcshift;
			dstdata[1] |= srcdata[1] << srcshift;
			dstdata[2] |= srcdata[2] << srcshift;
			dstdata[3] |= srcdata[3] << srcshift;
			dstdata[4] |= srcdata[4] << srcshift;
			dstdata[5] |= srcdata[5] << srcshift;
			dstdata[6] |= srcdata[6] << srcshift;
			dstdata[7] |= srcdata[7] << srcshift;
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) |= *(srcdata++) << srcshift;

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_noremap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		int srcshift),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] |= srcdata[8] << srcshift;
			dstdata[1] |= srcdata[7] << srcshift;
			dstdata[2] |= srcdata[6] << srcshift;
			dstdata[3] |= srcdata[5] << srcshift;
			dstdata[4] |= srcdata[4] << srcshift;
			dstdata[5] |= srcdata[3] << srcshift;
			dstdata[6] |= srcdata[2] << srcshift;
			dstdata[7] |= srcdata[1] << srcshift;
			dstdata += 8;
		}
		while (dstdata < end)
			*(dstdata++) |= *(srcdata--) << srcshift;

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_remap,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const pen_t *paldata,int srcshift),
{
	DATA_TYPE *end;

	srcmodulo -= srcwidth;
	dstmodulo -= srcwidth;

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			dstdata[0] = paldata[dstdata[0] | (srcdata[0] << srcshift)];
			dstdata[1] = paldata[dstdata[1] | (srcdata[1] << srcshift)];
			dstdata[2] = paldata[dstdata[2] | (srcdata[2] << srcshift)];
			dstdata[3] = paldata[dstdata[3] | (srcdata[3] << srcshift)];
			dstdata[4] = paldata[dstdata[4] | (srcdata[4] << srcshift)];
			dstdata[5] = paldata[dstdata[5] | (srcdata[5] << srcshift)];
			dstdata[6] = paldata[dstdata[6] | (srcdata[6] << srcshift)];
			dstdata[7] = paldata[dstdata[7] | (srcdata[7] << srcshift)];
			dstdata += 8;
			srcdata += 8;
		}
		while (dstdata < end)
		{
			*dstdata = paldata[*dstdata | (*(srcdata++) << srcshift)];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})

DECLARE(blockmove_NtoN_blend_remap_flipx,(
		const DATA_TYPE *srcdata,int srcwidth,int srcheight,int srcmodulo,
		DATA_TYPE *dstdata,int dstmodulo,
		const pen_t *paldata,int srcshift),
{
	DATA_TYPE *end;

	srcmodulo += srcwidth;
	dstmodulo -= srcwidth;
	/*srcdata += srcwidth-1;*/

	while (srcheight)
	{
		end = dstdata + srcwidth;
		while (dstdata <= end - 8)
		{
			srcdata -= 8;
			dstdata[0] = paldata[dstdata[0] | (srcdata[8] << srcshift)];
			dstdata[1] = paldata[dstdata[1] | (srcdata[7] << srcshift)];
			dstdata[2] = paldata[dstdata[2] | (srcdata[6] << srcshift)];
			dstdata[3] = paldata[dstdata[3] | (srcdata[5] << srcshift)];
			dstdata[4] = paldata[dstdata[4] | (srcdata[4] << srcshift)];
			dstdata[5] = paldata[dstdata[5] | (srcdata[3] << srcshift)];
			dstdata[6] = paldata[dstdata[6] | (srcdata[2] << srcshift)];
			dstdata[7] = paldata[dstdata[7] | (srcdata[1] << srcshift)];
			dstdata += 8;
		}
		while (dstdata < end)
		{
			*dstdata = paldata[*dstdata | (*(srcdata--) << srcshift)];
			dstdata++;
		}

		srcdata += srcmodulo;
		dstdata += dstmodulo;
		srcheight--;
	}
})





DECLARE(drawgfx_core,(
		struct mame_bitmap *dest,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color,
		struct mame_bitmap *pri_buffer,UINT32 pri_mask),
{
	int ox;
	int oy;
	int ex;
	int ey;


	/* check bounds */
	ox = sx;
	oy = sy;

	ex = sx + gfx->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;

	ey = sy + gfx->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	{
		UINT8 *sd = gfx->gfxdata + code * gfx->char_modulo;		/* source data */
		int sw = gfx->width;									/* source width */
		int sh = gfx->height;									/* source height */
		int sm = gfx->line_modulo;								/* source modulo */
		int ls = sx-ox;											/* left skip */
		int ts = sy-oy;											/* top skip */
		DATA_TYPE *dd = ((DATA_TYPE *)dest->line[sy]) + sx;		/* dest data */
		int dw = ex-sx+1;										/* dest width */
		int dh = ey-sy+1;										/* dest height */
		int dm = ((DATA_TYPE *)dest->line[1])-((DATA_TYPE *)dest->line[0]);	/* dest modulo */
		const pen_t *paldata = &gfx->colortable[gfx->color_granularity * color];
		UINT8 *pribuf = (pri_buffer) ? ((UINT8 *)pri_buffer->line[sy]) + sx : NULL;

		/* optimizations for 1:1 mapping */
		if (Machine->drv->color_table_len == 0 &&
			dest->depth == 16 &&
			paldata >= Machine->remapped_colortable &&
			paldata < Machine->remapped_colortable + Machine->drv->total_colors)
		{
			switch (transparency)
			{
				case TRANSPARENCY_NONE:
					transparency = TRANSPARENCY_NONE_RAW;
					color = paldata - Machine->remapped_colortable;
					break;
				case TRANSPARENCY_PEN:
					transparency = TRANSPARENCY_PEN_RAW;
					color = paldata - Machine->remapped_colortable;
					break;
				case TRANSPARENCY_PENS:
					transparency = TRANSPARENCY_PENS_RAW;
					color = paldata - Machine->remapped_colortable;
					break;
				case TRANSPARENCY_PEN_TABLE:
					transparency = TRANSPARENCY_PEN_TABLE_RAW;
					color = paldata - Machine->remapped_colortable;
					break;
			}
		}

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				if (gfx->flags & GFX_PACKED)
				{
					if (pribuf)
						BLOCKMOVEPRI(4toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask));
					else
						BLOCKMOVELU(4toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata));
				}
				else
				{
					if (pribuf)
						BLOCKMOVEPRI(8toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask));
					else
						BLOCKMOVELU(8toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata));
				}
				break;

			case TRANSPARENCY_NONE_RAW:
				if (gfx->flags & GFX_PACKED)
				{
					if (pribuf)
						BLOCKMOVERAWPRI(4toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask));
					else
						BLOCKMOVERAW(4toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color));
				}
				else
				{
					if (pribuf)
						BLOCKMOVERAWPRI(8toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask));
					else
						BLOCKMOVERAW(8toN_opaque,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color));
				}
				break;

			case TRANSPARENCY_PEN:
				if (gfx->flags & GFX_PACKED)
				{
					if (pribuf)
						BLOCKMOVEPRI(4toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
					else
						BLOCKMOVELU(4toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				}
				else
				{
					if (pribuf)
						BLOCKMOVEPRI(8toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
					else
						BLOCKMOVELU(8toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				}
				break;

			case TRANSPARENCY_PEN_RAW:
				if (gfx->flags & GFX_PACKED)
				{
					if (pribuf)
						BLOCKMOVERAWPRI(4toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask,transparent_color));
					else
						BLOCKMOVERAW(4toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,transparent_color));
				}
				else
				{
					if (pribuf)
						BLOCKMOVERAWPRI(8toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask,transparent_color));
					else
						BLOCKMOVERAW(8toN_transpen,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,transparent_color));
				}
				break;

			case TRANSPARENCY_PENS:
				if (pribuf)
					BLOCKMOVEPRI(8toN_transmask,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVELU(8toN_transmask,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_PENS_RAW:
				if (pribuf)
					BLOCKMOVERAWPRI(8toN_transmask,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVERAW(8toN_transmask,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,transparent_color));
				break;

			case TRANSPARENCY_COLOR:
				if (gfx->flags & GFX_PACKED)
				{
					if (pribuf)
						BLOCKMOVEPRI(4toN_transcolor,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,Machine->game_colortable + (paldata - Machine->remapped_colortable),transparent_color));
					else
						BLOCKMOVELU(4toN_transcolor,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,Machine->game_colortable + (paldata - Machine->remapped_colortable),transparent_color));
				}
				else
				{
					if (pribuf)
						BLOCKMOVEPRI(8toN_transcolor,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,Machine->game_colortable + (paldata - Machine->remapped_colortable),transparent_color));
					else
						BLOCKMOVELU(8toN_transcolor,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,Machine->game_colortable + (paldata - Machine->remapped_colortable),transparent_color));
				}
				break;

			case TRANSPARENCY_PEN_TABLE:
				if (pribuf)
					BLOCKMOVEPRI(8toN_pen_table,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVELU(8toN_pen_table,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_PEN_TABLE_RAW:
				if (pribuf)
					BLOCKMOVERAWPRI(8toN_pen_table,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVERAW(8toN_pen_table,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,transparent_color));
				break;

			case TRANSPARENCY_BLEND_RAW:
				if (pribuf)
					BLOCKMOVERAWPRI(8toN_transblend,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVERAW(8toN_transblend,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,color,transparent_color));
				break;

			case TRANSPARENCY_ALPHAONE:
				if (pribuf)
					BLOCKMOVEPRI(8toN_alphaone,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color & 0xff, (transparent_color>>8) & 0xff));
				else
					BLOCKMOVELU(8toN_alphaone,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color & 0xff, (transparent_color>>8) & 0xff));
				break;

			case TRANSPARENCY_ALPHA:
				if (pribuf)
					BLOCKMOVEPRI(8toN_alpha,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVELU(8toN_alpha,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				break;

			case TRANSPARENCY_ALPHARANGE:
				if (pribuf)
					BLOCKMOVEPRI(8toN_alpharange,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,pribuf,pri_mask,transparent_color));
				else
					BLOCKMOVELU(8toN_alpharange,(sd,sw,sh,sm,ls,ts,flipx,flipy,dd,dw,dh,dm,paldata,transparent_color));
				break;

			default:
				if (pribuf)
					usrintf_showmessage("pdrawgfx pen mode not supported");
				else
					usrintf_showmessage("drawgfx pen mode not supported");
				break;
		}
	}
})

DECLARE(copybitmap_core,(
		struct mame_bitmap *dest,struct mame_bitmap *src,
		int flipx,int flipy,int sx,int sy,
		const struct rectangle *clip,int transparency,int transparent_color),
{
	int ox;
	int oy;
	int ex;
	int ey;


	/* check bounds */
	ox = sx;
	oy = sy;

	ex = sx + src->width-1;
	if (sx < 0) sx = 0;
	if (clip && sx < clip->min_x) sx = clip->min_x;
	if (ex >= dest->width) ex = dest->width-1;
	if (clip && ex > clip->max_x) ex = clip->max_x;
	if (sx > ex) return;

	ey = sy + src->height-1;
	if (sy < 0) sy = 0;
	if (clip && sy < clip->min_y) sy = clip->min_y;
	if (ey >= dest->height) ey = dest->height-1;
	if (clip && ey > clip->max_y) ey = clip->max_y;
	if (sy > ey) return;

	{
		DATA_TYPE *sd = ((DATA_TYPE *)src->line[0]);							/* source data */
		int sw = ex-sx+1;														/* source width */
		int sh = ey-sy+1;														/* source height */
		int sm = ((DATA_TYPE *)src->line[1])-((DATA_TYPE *)src->line[0]);		/* source modulo */
		DATA_TYPE *dd = ((DATA_TYPE *)dest->line[sy]) + sx;						/* dest data */
		int dm = ((DATA_TYPE *)dest->line[1])-((DATA_TYPE *)dest->line[0]);		/* dest modulo */

		if (flipx)
		{
			/*if ((sx-ox) == 0) sd += gfx->width - sw;*/
			sd += src->width -1 -(sx-ox);
		}
		else
			sd += (sx-ox);

		if (flipy)
		{
			/*if ((sy-oy) == 0) sd += sm * (gfx->height - sh);*/
			/*dd += dm * (sh - 1);*/
			/*dm = -dm;*/
			sd += sm * (src->height -1 -(sy-oy));
			sm = -sm;
		}
		else
			sd += sm * (sy-oy);

		switch (transparency)
		{
			case TRANSPARENCY_NONE:
				BLOCKMOVE(NtoN_opaque_remap,flipx,(sd,sw,sh,sm,dd,dm,Machine->pens));
				break;

			case TRANSPARENCY_NONE_RAW:
				BLOCKMOVE(NtoN_opaque_noremap,flipx,(sd,sw,sh,sm,dd,dm));
				break;

			case TRANSPARENCY_PEN_RAW:
				BLOCKMOVE(NtoN_transpen_noremap,flipx,(sd,sw,sh,sm,dd,dm,transparent_color));
				break;

			case TRANSPARENCY_BLEND:
				BLOCKMOVE(NtoN_blend_remap,flipx,(sd,sw,sh,sm,dd,dm,Machine->pens,transparent_color));
				break;

			case TRANSPARENCY_BLEND_RAW:
				BLOCKMOVE(NtoN_blend_noremap,flipx,(sd,sw,sh,sm,dd,dm,transparent_color));
				break;

			default:
				usrintf_showmessage("copybitmap pen mode not supported");
				break;
		}
	}
})

DECLARE(copyrozbitmap_core,(struct mame_bitmap *bitmap,struct mame_bitmap *srcbitmap,
		UINT32 startx,UINT32 starty,int incxx,int incxy,int incyx,int incyy,int wraparound,
		const struct rectangle *clip,int transparency,int transparent_color,UINT32 priority),
{
	UINT32 cx;
	UINT32 cy;
	int x;
	int sx;
	int sy;
	int ex;
	int ey;
	const int xmask = srcbitmap->width-1;
	const int ymask = srcbitmap->height-1;
	const int widthshifted = srcbitmap->width << 16;
	const int heightshifted = srcbitmap->height << 16;
	DATA_TYPE *dest;


	if (clip)
	{
		startx += clip->min_x * incxx + clip->min_y * incyx;
		starty += clip->min_x * incxy + clip->min_y * incyy;

		sx = clip->min_x;
		sy = clip->min_y;
		ex = clip->max_x;
		ey = clip->max_y;
	}
	else
	{
		sx = 0;
		sy = 0;
		ex = bitmap->width-1;
		ey = bitmap->height-1;
	}


	if (incxy == 0 && incyx == 0 && !wraparound)
	{
		/* optimized loop for the not rotated case */

		if (incxx == 0x10000)
		{
			/* optimized loop for the not zoomed case */

			/* startx is unsigned */
			startx = ((INT32)startx) >> 16;

			if (startx >= srcbitmap->width)
			{
				sx += -startx;
				startx = 0;
			}

			if (sx <= ex)
			{
				while (sy <= ey)
				{
					if (starty < heightshifted)
					{
						x = sx;
						cx = startx;
						cy = starty >> 16;
						dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
						if (priority)
						{
							UINT8 *pri = ((UINT8 *)priority_bitmap->line[sy]) + sx;
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < srcbitmap->width)
							{
								int c = src[cx];

								if (c != transparent_color)
								{
									*dest = c;
									*pri |= priority;
								}

								cx++;
								x++;
								dest++;
								pri++;
							}
						}
						else
						{
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < srcbitmap->width)
							{
								int c = src[cx];

								if (c != transparent_color)
									*dest = c;

								cx++;
								x++;
								dest++;
							}
						}
					}
					starty += incyy;
					sy++;
				}
			}
		}
		else
		{
			while (startx >= widthshifted && sx <= ex)
			{
				startx += incxx;
				sx++;
			}

			if (sx <= ex)
			{
				while (sy <= ey)
				{
					if (starty < heightshifted)
					{
						x = sx;
						cx = startx;
						cy = starty >> 16;
						dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
						if (priority)
						{
							UINT8 *pri = ((UINT8 *)priority_bitmap->line[sy]) + sx;
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < widthshifted)
							{
								int c = src[cx >> 16];

								if (c != transparent_color)
								{
									*dest = c;
									*pri |= priority;
								}

								cx += incxx;
								x++;
								dest++;
								pri++;
							}
						}
						else
						{
							DATA_TYPE *src = (DATA_TYPE *)srcbitmap->line[cy];

							while (x <= ex && cx < widthshifted)
							{
								int c = src[cx >> 16];

								if (c != transparent_color)
									*dest = c;

								cx += incxx;
								x++;
								dest++;
							}
						}
					}
					starty += incyy;
					sy++;
				}
			}
		}
	}
	else
	{
		if (wraparound)
		{
			/* plot with wraparound */
			while (sy <= ey)
			{
				x = sx;
				cx = startx;
				cy = starty;
				dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
				if (priority)
				{
					UINT8 *pri = ((UINT8 *)priority_bitmap->line[sy]) + sx;

					while (x <= ex)
					{
						int c = ((DATA_TYPE *)srcbitmap->line[(cy >> 16) & ymask])[(cx >> 16) & xmask];

						if (c != transparent_color)
						{
							*dest = c;
							*pri |= priority;
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
						pri++;
					}
				}
				else
				{
					while (x <= ex)
					{
						int c = ((DATA_TYPE *)srcbitmap->line[(cy >> 16) & ymask])[(cx >> 16) & xmask];

						if (c != transparent_color)
							*dest = c;

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
					}
				}
				startx += incyx;
				starty += incyy;
				sy++;
			}
		}
		else
		{
			while (sy <= ey)
			{
				x = sx;
				cx = startx;
				cy = starty;
				dest = ((DATA_TYPE *)bitmap->line[sy]) + sx;
				if (priority)
				{
					UINT8 *pri = ((UINT8 *)priority_bitmap->line[sy]) + sx;

					while (x <= ex)
					{
						if (cx < widthshifted && cy < heightshifted)
						{
							int c = ((DATA_TYPE *)srcbitmap->line[cy >> 16])[cx >> 16];

							if (c != transparent_color)
							{
								*dest = c;
								*pri |= priority;
							}
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
						pri++;
					}
				}
				else
				{
					while (x <= ex)
					{
						if (cx < widthshifted && cy < heightshifted)
						{
							int c = ((DATA_TYPE *)srcbitmap->line[cy >> 16])[cx >> 16];

							if (c != transparent_color)
								*dest = c;
						}

						cx += incxx;
						cy += incxy;
						x++;
						dest++;
					}
				}
				startx += incyx;
				starty += incyy;
				sy++;
			}
		}
	}
})

DECLAREG(draw_scanline, (
		struct mame_bitmap *bitmap,int x,int y,int length,
		const DATA_TYPE *src,pen_t *pens,int transparent_pen),
{
	/* 8bpp destination */
	if (bitmap->depth == 8)
	{
		int dy = bitmap->rowpixels;
		UINT8 *dst = (UINT8 *)bitmap->base + y * dy + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = pens[*src++];
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = pens[spixel];
					dst += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = *src++;
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = spixel;
					dst += xadv;
				}
		}
	}

	/* 16bpp destination */
	else if(bitmap->depth == 15 || bitmap->depth == 16)
	{
		int dy = bitmap->rowpixels;
		UINT16 *dst = (UINT16 *)bitmap->base + y * dy + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = pens[*src++];
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = pens[spixel];
					dst += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = *src++;
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = spixel;
					dst += xadv;
				}
		}
	}

	/* 32bpp destination */
	else
	{
		int dy = bitmap->rowpixels;
		UINT32 *dst = (UINT32 *)bitmap->base + y * dy + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = pens[*src++];
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = pens[spixel];
					dst += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dst = *src++;
					dst += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
						*dst = spixel;
					dst += xadv;
				}
		}
	}
})

DECLAREG(pdraw_scanline, (
		struct mame_bitmap *bitmap,int x,int y,int length,
		const DATA_TYPE *src,pen_t *pens,int transparent_pen,int pri),
{
	/* 8bpp destination */
	if (bitmap->depth == 8)
	{
		int dy = bitmap->rowpixels;
		int dyp = priority_bitmap->rowpixels;
		UINT8 *dsti = (UINT8 *)bitmap->base + y * dy + x;
		UINT8 *dstp = (UINT8 *)priority_bitmap->base + y * dyp + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = pens[*src++];
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = pens[spixel];
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = *src++;
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = spixel;
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}
	}

	/* 16bpp destination */
	else if(bitmap->depth == 15 || bitmap->depth == 16)
	{
		int dy = bitmap->rowpixels;
		int dyp = priority_bitmap->rowpixels;
		UINT16 *dsti = (UINT16 *)bitmap->base + y * dy + x;
		UINT8 *dstp = (UINT8 *)priority_bitmap->base + y * dyp + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = pens[*src++];
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = pens[spixel];
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = *src++;
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = spixel;
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}
	}

	/* 32bpp destination */
	else
	{
		int dy = bitmap->rowpixels;
		int dyp = priority_bitmap->rowpixels;
		UINT32 *dsti = (UINT32 *)bitmap->base + y * dy + x;
		UINT8 *dstp = (UINT8 *)priority_bitmap->base + y * dyp + x;
		int xadv = 1;

		/* with pen lookups */
		if (pens)
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = pens[*src++];
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = pens[spixel];
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}

		/* without pen lookups */
		else
		{
			if (transparent_pen == -1)
				while (length--)
				{
					*dsti = *src++;
					*dstp = pri;
					dsti += xadv;
					dstp += xadv;
				}
			else
				while (length--)
				{
					UINT32 spixel = *src++;
					if (spixel != transparent_pen)
					{
						*dsti = spixel;
						*dstp = pri;
					}
					dsti += xadv;
					dstp += xadv;
				}
		}
	}
}
)

DECLAREG(extract_scanline, (
		struct mame_bitmap *bitmap,int x,int y,int length,
		DATA_TYPE *dst),
{
	/* 8bpp destination */
	if (bitmap->depth == 8)
	{
		int dy = bitmap->rowpixels;
		UINT8 *src = (UINT8 *)bitmap->base + y * dy + x;
		int xadv = 1;

		while (length--)
		{
			*dst++ = *src;
			src += xadv;
		}
	}

	/* 16bpp destination */
	else if(bitmap->depth == 15 || bitmap->depth == 16)
	{
		int dy = bitmap->rowpixels;
		UINT16 *src = (UINT16 *)bitmap->base + y * dy + x;
		int xadv = 1;

		while (length--)
		{
			*dst++ = *src;
			src += xadv;
		}
	}

	/* 32bpp destination */
	else
	{
		int dy = bitmap->rowpixels;
		UINT32 *src = (UINT32 *)bitmap->base + y * dy + x;
		int xadv = 1;

		while (length--)
		{
			*dst++ = *src;
			src += xadv;
		}
	}
})

#endif /* DECLARE */
