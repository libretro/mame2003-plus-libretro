/***************************************************************************

	Cave CV1000 blitter (Altera Cyclone EP1C12 FPGA)

	From current MAME (src/mame/cave/cv1k_v.cpp and the blit functions of
	cv1k_v_in.ipp and cv1k_v_pixel.ipp: David Haywood, Luca Elia,
	MetalliC). The blitter timings are based on "Blitter Research by
	buffi" (https://cave.buffis.com/docs/CV1000_Blitter_Research_by_buffi.pdf).

	Differences from MAME:
	- the VRAM is kept as 16-bit pixels (t-rrrrr-ggggg-bbbbb) instead of
	  32-bit ones: the blitter only ever stores 5 bits per gun and the
	  transparency bit, so the result is the same with half the memory;
	- the blit list runs when the game starts it, not in a thread: the
	  game never reads the VRAM, so the screen is the same; the busy time
	  the game waits on is computed as in MAME;
	- the drawing is clipped to the VRAM (MAME writes outside it when the
	  clip window goes past an edge).

***************************************************************************/

#include <string.h>
#include "driver.h"
#include "state.h"
#include "cpu/sh3/sh3.h"
#include "includes/cv1k.h"

#define VRAM_W		0x2000
#define VRAM_H		0x1000

#define CV1K_VRAM_CLK_NANOSEC				13
#define CV1K_SRAM_CLK_NANOSEC				20
#define CV1K_UPLOAD_CHUNK_GAP_NANOSEC		1130
#define CV1K_VRAM_H_LINE_PERIOD_NANOSEC		63600
#define CV1K_VRAM_H_LINE_DURATION_NANOSEC	2160
#define CV1K_DRAW_OPERATION_SIZE_BYTES		20
#define CV1K_CLIP_OPERATION_SIZE_BYTES		2
#define CV1K_CLIP_MARGIN					32
#define OPERATION_CHUNK_SIZE_BYTES			64
#define OPERATION_READ_CHUNK_INTERVAL_NS	700

typedef struct
{
	int min_x, max_x, min_y, max_y;
} cv1k_rect;

static UINT16 *cv1k_vram;
static UINT32 gfx_addr, gfx_scroll_x, gfx_scroll_y, gfx_clip_x, gfx_clip_y;
static UINT32 gfx_addr_shadowcopy, gfx_clip_x_shadowcopy, gfx_clip_y_shadowcopy;
static cv1k_rect blit_clip;			/* the clip of the blit list (also for the timing) */
static cv1k_rect draw_clip;			/* the same, inside the VRAM */
static UINT8 blitter_busy;
static UINT32 blit_delay_ns;
static UINT16 blit_idle_op_bytes;
static void *blitter_delay_timer;
static INT32 firmware_pos;
static UINT8 firmware_port;
static UINT8 firmware_checksum;

static UINT8 colrtable[0x20][0x40];
static UINT8 colrtable_rev[0x20][0x40];
static UINT8 colrtable_add[0x20][0x20];


/***************************************************************************
	blit functions (cv1k_v_in.ipp, cv1k_v_pixel.ipp)
***************************************************************************/

#define T(a, b)		colrtable[a][b]
#define TR(a, b)	colrtable_rev[a][b]
#define A(a, b)		colrtable_add[a][b]

#define BLIT_PARAMS		const cv1k_rect *clip, int src_x, int src_y, const int dst_x_start, const int dst_y_start, \
						int dimx, int dimy, const int flipy, const int s_alpha, const int d_alpha, \
						const int tint_r, const int tint_g, const int tint_b

typedef void (*blitfunction)(BLIT_PARAMS);

/* s = source, d = destination, c = clr0 of MAME; the modes are constants,
   the compiler keeps only the code of one */
#define BLEND(SMODE, DMODE)																			\
	if (SMODE == 0)																					\
	{																								\
		if (DMODE == 0)																				\
		{																							\
			sr = A(T(s_alpha, sr), T(d_alpha, dr));												\
			sg = A(T(s_alpha, sg), T(d_alpha, dg));												\
			sb = A(T(s_alpha, sb), T(d_alpha, db));												\
		}																							\
		else if (DMODE == 1)																		\
		{																							\
			sr = A(T(s_alpha, sr), T(sr, dr));														\
			sg = A(T(s_alpha, sg), T(sg, dg));														\
			sb = A(T(s_alpha, sb), T(sb, db));														\
		}																							\
		else if (DMODE == 5)																		\
		{																							\
			sr = A(T(s_alpha, sr), TR(sr, dr));														\
			sg = A(T(s_alpha, sg), TR(sg, dg));														\
			sb = A(T(s_alpha, sb), TR(sb, db));														\
		}																							\
		else																						\
		{																							\
			cr = T(s_alpha, sr); cg = T(s_alpha, sg); cb = T(s_alpha, sb);							\
			BLEND_DEST(DMODE)																		\
		}																							\
	}																								\
	else if (SMODE == 2)																			\
	{																								\
		if (DMODE == 0)																				\
		{																							\
			sr = A(T(dr, sr), T(d_alpha, dr));														\
			sg = A(T(dg, sg), T(d_alpha, dg));														\
			sb = A(T(db, sb), T(d_alpha, db));														\
		}																							\
		else																						\
		{																							\
			cr = T(dr, sr); cg = T(dg, sg); cb = T(db, sb);										\
			BLEND_DEST(DMODE)																		\
		}																							\
	}																								\
	else																							\
	{																								\
		if (SMODE == 1)																				\
			{ cr = T(sr, sr); cg = T(sg, sg); cb = T(sb, sb); }										\
		else if (SMODE == 3 || SMODE == 7)															\
			{ cr = sr; cg = sg; cb = sb; }															\
		else if (SMODE == 4)																		\
			{ cr = TR(s_alpha, sr); cg = TR(s_alpha, sg); cb = TR(s_alpha, sb); }					\
		else if (SMODE == 5)																		\
			{ cr = TR(sr, sr); cg = TR(sg, sg); cb = TR(sb, sb); }									\
		else																						\
			{ cr = TR(dr, sr); cg = TR(dg, sg); cb = TR(db, sb); }									\
		if (DMODE == 0)																				\
		{																							\
			sr = A(cr, T(dr, d_alpha));																\
			sg = A(cg, T(dg, d_alpha));																\
			sb = A(cb, T(db, d_alpha));																\
		}																							\
		else																						\
			BLEND_DEST(DMODE)																		\
	}

/* the destination part shared by the modes with a clr0; mode 2 uses the
   red of clr0 for the three guns, as MAME does */
#define BLEND_DEST(DMODE)																			\
	if (DMODE == 1)																					\
	{																								\
		sr = A(cr, T(sr, dr));																		\
		sg = A(cg, T(sg, dg));																		\
		sb = A(cb, T(sb, db));																		\
	}																								\
	else if (DMODE == 2)																			\
	{																								\
		sr = A(cr, T(dr, dr));																		\
		sg = A(cr, T(dg, dg));																		\
		sb = A(cr, T(db, db));																		\
	}																								\
	else if (DMODE == 3 || DMODE == 7)																\
	{																								\
		sr = A(cr, dr);																				\
		sg = A(cg, dg);																				\
		sb = A(cb, db);																				\
	}																								\
	else if (DMODE == 4)																			\
	{																								\
		sr = A(cr, TR(d_alpha, dr));																\
		sg = A(cg, TR(d_alpha, dg));																\
		sb = A(cb, TR(d_alpha, db));																\
	}																								\
	else if (DMODE == 5)																			\
	{																								\
		sr = A(cr, TR(sr, dr));																		\
		sg = A(cg, TR(sg, dg));																		\
		sb = A(cb, TR(sb, db));																		\
	}																								\
	else if (DMODE == 6)																			\
	{																								\
		sr = A(cr, TR(dr, dr));																		\
		sg = A(cg, TR(dg, dg));																		\
		sb = A(cb, TR(db, db));																		\
	}

/* SIMPLE: copy the pixels (REALLY_SIMPLE of MAME) */
#define DEFINE_BLIT(NAME, FLIPX, TINT, TRANSPARENT, BLENDED, SMODE, DMODE, SIMPLE)					\
static void NAME(BLIT_PARAMS)																		\
{																									\
	int yf, y, starty = 0, startx = 0;																\
	const int dst_y_end = dst_y_start + dimy;														\
	int dst_x_end;																					\
																									\
	if (FLIPX)																						\
		src_x += (dimx - 1);																		\
	if (flipy)	{ yf = -1; src_y += (dimy - 1); }													\
	else		{ yf = +1; }																		\
																									\
	if (dst_y_start < clip->min_y)																	\
		starty = clip->min_y - dst_y_start;															\
	if (dst_y_end > clip->max_y)																	\
		dimy -= (dst_y_end - 1) - clip->max_y;														\
																									\
	/* a source that wraps around the VRAM edge is not drawn */										\
	if (FLIPX)																						\
	{																								\
		if ((src_x & 0x1fff) < ((src_x - (dimx - 1)) & 0x1fff))										\
			return;																					\
	}																								\
	else																							\
	{																								\
		if ((src_x & 0x1fff) > ((src_x + (dimx - 1)) & 0x1fff))										\
			return;																					\
	}																								\
																									\
	dst_x_end = dst_x_start + dimx;																	\
	if (dst_x_start < clip->min_x)																	\
		startx = clip->min_x - dst_x_start;															\
	if (dst_x_end > clip->max_x)																	\
		dimx -= (dst_x_end - 1) - clip->max_x;														\
																									\
	for (y = starty; y < dimy; y++)																	\
	{																								\
		UINT16 *bmp = &cv1k_vram[(dst_y_start + y) * VRAM_W + dst_x_start + startx];				\
		const UINT16 *gfx2 = &cv1k_vram[((src_y + yf * y) & 0x0fff) * VRAM_W						\
										+ (FLIPX ? (src_x - startx) : (src_x + startx))];			\
		int count = dimx - startx;																	\
																									\
		while (count-- > 0)																			\
		{																							\
			const UINT16 pen = *gfx2;																\
			if (!TRANSPARENT || (pen & 0x8000))														\
			{																						\
				if (SIMPLE)																			\
					*bmp = pen;																		\
				else																				\
				{																					\
					int sr = (pen >> 10) & 0x1f, sg = (pen >> 5) & 0x1f, sb = pen & 0x1f;			\
					if (TINT)																		\
					{																				\
						sr = T(sr, tint_r);															\
						sg = T(sg, tint_g);															\
						sb = T(sb, tint_b);															\
					}																				\
					if (BLENDED)																	\
					{																				\
						const UINT16 dst = *bmp;													\
						const int dr = (dst >> 10) & 0x1f, dg = (dst >> 5) & 0x1f, db = dst & 0x1f;	\
						int cr, cg, cb;																\
						BLEND(SMODE, DMODE)															\
						(void)cr; (void)cg; (void)cb; (void)dr; (void)dg; (void)db;				\
					}																				\
					*bmp = (sr << 10) | (sg << 5) | sb | (pen & 0x8000);							\
				}																					\
			}																						\
			bmp++;																					\
			if (FLIPX) gfx2--; else gfx2++;															\
		}																							\
	}																								\
}

#define BLIT_NAME(f, ti, tr, s, d)		draw_sprite_f##f##_ti##ti##_tr##tr##_s##s##_d##d

#define DEFINE_BLIT_D(f, ti, tr, d)																	\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 0, d), f, ti, tr, 1, 0, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 1, d), f, ti, tr, 1, 1, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 2, d), f, ti, tr, 1, 2, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 3, d), f, ti, tr, 1, 3, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 4, d), f, ti, tr, 1, 4, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 5, d), f, ti, tr, 1, 5, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 6, d), f, ti, tr, 1, 6, d, 0)									\
	DEFINE_BLIT(BLIT_NAME(f, ti, tr, 7, d), f, ti, tr, 1, 7, d, 0)

#define DEFINE_BLIT_SET(f, ti, tr)																	\
	DEFINE_BLIT_D(f, ti, tr, 0)																		\
	DEFINE_BLIT_D(f, ti, tr, 1)																		\
	DEFINE_BLIT_D(f, ti, tr, 2)																		\
	DEFINE_BLIT_D(f, ti, tr, 3)																		\
	DEFINE_BLIT_D(f, ti, tr, 4)																		\
	DEFINE_BLIT_D(f, ti, tr, 5)																		\
	DEFINE_BLIT_D(f, ti, tr, 6)																		\
	DEFINE_BLIT_D(f, ti, tr, 7)

#define BLIT_TABLE_D(f, ti, tr, d)																	\
	BLIT_NAME(f, ti, tr, 0, d), BLIT_NAME(f, ti, tr, 1, d), BLIT_NAME(f, ti, tr, 2, d), BLIT_NAME(f, ti, tr, 3, d), \
	BLIT_NAME(f, ti, tr, 4, d), BLIT_NAME(f, ti, tr, 5, d), BLIT_NAME(f, ti, tr, 6, d), BLIT_NAME(f, ti, tr, 7, d)

#define BLIT_TABLE(fl, ti, tr)																		\
	static const blitfunction blit_funcs_f##fl##_ti##ti##_tr##tr[64] =								\
	{																								\
		BLIT_TABLE_D(fl, ti, tr, 0), BLIT_TABLE_D(fl, ti, tr, 1), BLIT_TABLE_D(fl, ti, tr, 2), BLIT_TABLE_D(fl, ti, tr, 3), \
		BLIT_TABLE_D(fl, ti, tr, 4), BLIT_TABLE_D(fl, ti, tr, 5), BLIT_TABLE_D(fl, ti, tr, 6), BLIT_TABLE_D(fl, ti, tr, 7) \
	};

/* blended: flip x, tint, transparency, source and destination modes */
DEFINE_BLIT_SET(0, 0, 0)
DEFINE_BLIT_SET(0, 0, 1)
DEFINE_BLIT_SET(0, 1, 0)
DEFINE_BLIT_SET(0, 1, 1)
DEFINE_BLIT_SET(1, 0, 0)
DEFINE_BLIT_SET(1, 0, 1)
DEFINE_BLIT_SET(1, 1, 0)
DEFINE_BLIT_SET(1, 1, 1)

BLIT_TABLE(0, 0, 0)
BLIT_TABLE(0, 0, 1)
BLIT_TABLE(0, 1, 0)
BLIT_TABLE(0, 1, 1)
BLIT_TABLE(1, 0, 0)
BLIT_TABLE(1, 0, 1)
BLIT_TABLE(1, 1, 0)
BLIT_TABLE(1, 1, 1)

/* tinted, not blended */
DEFINE_BLIT(draw_sprite_f0_ti1_tr1_plain, 0, 1, 1, 0, 0, 0, 0)
DEFINE_BLIT(draw_sprite_f0_ti1_tr0_plain, 0, 1, 0, 0, 0, 0, 0)
DEFINE_BLIT(draw_sprite_f1_ti1_tr1_plain, 1, 1, 1, 0, 0, 0, 0)
DEFINE_BLIT(draw_sprite_f1_ti1_tr0_plain, 1, 1, 0, 0, 0, 0, 0)

/* neither tinted nor blended: copies */
DEFINE_BLIT(draw_sprite_f0_ti0_tr1_simple, 0, 0, 1, 0, 0, 0, 1)
DEFINE_BLIT(draw_sprite_f0_ti0_tr0_simple, 0, 0, 0, 0, 0, 0, 1)
DEFINE_BLIT(draw_sprite_f1_ti0_tr1_simple, 1, 0, 1, 0, 0, 0, 1)
DEFINE_BLIT(draw_sprite_f1_ti0_tr0_simple, 1, 0, 0, 0, 0, 0, 1)


/***************************************************************************
	blit list
***************************************************************************/

/* the 16-bit word at a byte address of the main RAM */
#define RAM_WORD(addr)	(((UINT16 *)cv1k_ram)[WORD_XOR_BE((addr) & (cv1k_ram_size - 1)) >> 1])

static INLINE UINT16 read_next_word(UINT32 *addr)
{
	UINT16 data = RAM_WORD(*addr);
	*addr += 2;
	return data;
}

static void set_clip(int cliptype)
{
	if (cliptype)
	{
		blit_clip.min_x = (INT32)(gfx_clip_x_shadowcopy - CV1K_CLIP_MARGIN);
		blit_clip.max_x = (INT32)(gfx_clip_x_shadowcopy + 320 - 1 + CV1K_CLIP_MARGIN);
		blit_clip.min_y = (INT32)(gfx_clip_y_shadowcopy - CV1K_CLIP_MARGIN);
		blit_clip.max_y = (INT32)(gfx_clip_y_shadowcopy + 240 - 1 + CV1K_CLIP_MARGIN);
	}
	else
	{
		blit_clip.min_x = 0;
		blit_clip.max_x = VRAM_W - 1;
		blit_clip.min_y = 0;
		blit_clip.max_y = VRAM_H - 1;
	}
	draw_clip = blit_clip;
	if (draw_clip.min_x < 0) draw_clip.min_x = 0;
	if (draw_clip.min_y < 0) draw_clip.min_y = 0;
	if (draw_clip.max_x > VRAM_W - 1) draw_clip.max_x = VRAM_W - 1;
	if (draw_clip.max_y > VRAM_H - 1) draw_clip.max_y = VRAM_H - 1;
}

static void idle_blitter(UINT8 operation_size_bytes)
{
	blit_idle_op_bytes += operation_size_bytes;
	if (blit_idle_op_bytes >= OPERATION_CHUNK_SIZE_BYTES)
	{
		blit_idle_op_bytes -= OPERATION_CHUNK_SIZE_BYTES;
		blit_delay_ns += OPERATION_READ_CHUNK_INTERVAL_NS;
	}
}

/* the number of VRAM rows (32 x 32 pixels) a draw touches */
static UINT16 calculate_vram_accesses(UINT16 start_x, UINT16 start_y, UINT16 dimx, UINT16 dimy)
{
	int x_rows = 0;
	int num_vram_rows = 0;
	int x_pixels, y_pixels;

	for (x_pixels = dimx; x_pixels > 0; x_pixels -= 32)
	{
		x_rows++;
		if (((start_x & 31) + (x_pixels < 32 ? x_pixels : 32)) > 32)
			x_rows++;
	}
	for (y_pixels = dimy; y_pixels > 0; y_pixels -= 32)
	{
		num_vram_rows += x_rows;
		if (((start_y & 31) + (y_pixels < 32 ? y_pixels : 32)) > 32)
			num_vram_rows += x_rows;
	}
	return num_vram_rows;
}

/* upload: the timing (gfx_upload_shadow_copy of MAME) */
static void gfx_upload_timing(UINT32 *addr)
{
	const UINT32 chunk_offset = (*addr - (gfx_addr & 0x1fffffff)) & (OPERATION_CHUNK_SIZE_BYTES - 1);
	UINT32 dimx, dimy;
	int num_bytes, num_sram_clk, num_chunk_gaps;

	*addr += 12;
	dimx = (read_next_word(addr) & 0x1fff) + 1;
	dimy = (read_next_word(addr) & 0x0fff) + 1;
	*addr += dimx * dimy * 2;

	/* mostly the main RAM reads: the blitter takes the bus from the SH-3,
	   each bus cycle is 2 CPU cycles */
	num_bytes = 16 + dimx * dimy * 2;
	num_sram_clk = num_bytes / 4;
	sh3_ICount -= num_sram_clk * 2;
	blit_delay_ns += num_sram_clk * CV1K_SRAM_CLK_NANOSEC;
	/* a bus request between each 64 byte chunk */
	num_chunk_gaps = (chunk_offset + num_bytes) / OPERATION_CHUNK_SIZE_BYTES;
	blit_delay_ns += num_chunk_gaps * CV1K_UPLOAD_CHUNK_GAP_NANOSEC;
	blit_idle_op_bytes = 0;
}

static void gfx_upload(UINT32 *addr)
{
	UINT32 dst_x_start, dst_y_start, dimx, dimy, x, y;

	*addr += 8;
	dst_x_start = read_next_word(addr) & 0x1fff;
	dst_y_start = read_next_word(addr) & 0x0fff;
	dimx = (read_next_word(addr) & 0x1fff) + 1;
	dimy = (read_next_word(addr) & 0x0fff) + 1;

	for (y = 0; y < dimy; y++)
	{
		UINT32 row = (dst_y_start + y) & (VRAM_H - 1);
		for (x = 0; x < dimx; x++)
		{
			const UINT16 pendat = read_next_word(addr);
			UINT32 col = dst_x_start + x;
			/* VRAM is 1555 like the uploaded data */
			if (col < VRAM_W)
				cv1k_vram[row * VRAM_W + col] = pendat;
		}
	}
}

/* draw: the timing (gfx_draw_shadow_copy of MAME) */
static void gfx_draw_timing(UINT32 *addr)
{
	UINT16 src_x_start, src_y_start, dst_x_start, dst_y_start, src_dimx, src_dimy;
	UINT16 dst_x_end, dst_y_end, dst_dimx, dst_dimy;
	int src_num_vram_rows, dst_num_vram_rows;
	UINT32 num_vram_clk;

	*addr += 4;
	src_x_start = read_next_word(addr);
	src_y_start = read_next_word(addr);
	dst_x_start = read_next_word(addr);
	dst_y_start = read_next_word(addr);
	src_dimx = (read_next_word(addr) & 0x1fff) + 1;
	src_dimy = (read_next_word(addr) & 0x0fff) + 1;
	*addr += 4;

	dst_x_end = dst_x_start + src_dimx - 1;
	dst_y_end = dst_y_start + src_dimy - 1;

	/* sprites fully outside of the clip area are not drawn */
	if (!(blit_clip.min_x <= (int)dst_x_end && blit_clip.max_x >= (int)dst_x_start &&
		  blit_clip.min_y <= (int)dst_y_end && blit_clip.max_y >= (int)dst_y_start))
	{
		idle_blitter(CV1K_DRAW_OPERATION_SIZE_BYTES);
		return;
	}

	/* the timing only counts the area written */
	if (dst_x_start < (UINT16)blit_clip.min_x) dst_x_start = (UINT16)blit_clip.min_x;
	if (dst_y_start < (UINT16)blit_clip.min_y) dst_y_start = (UINT16)blit_clip.min_y;
	if (dst_x_end > (UINT16)blit_clip.max_x) dst_x_end = (UINT16)blit_clip.max_x;
	if (dst_y_end > (UINT16)blit_clip.max_y) dst_y_end = (UINT16)blit_clip.max_y;
	src_dimx = dst_x_end - dst_x_start + 1;
	src_dimy = dst_y_end - dst_y_start + 1;

	blit_idle_op_bytes = 0;

	src_num_vram_rows = calculate_vram_accesses(src_x_start, src_y_start, src_dimx, src_dimy);
	dst_num_vram_rows = calculate_vram_accesses(dst_x_start, dst_y_start, src_dimx, src_dimy);

	/* draws are done 4 pixels at a time */
	dst_x_start -= dst_x_start & 3;
	dst_x_end += (4 - ((dst_x_end + 1) & 3)) & 3;
	dst_dimx = dst_x_end - dst_x_start + 1;
	dst_dimy = dst_y_end - dst_y_start + 1;

	num_vram_clk = src_dimx * src_dimy / 4 + dst_dimx * dst_dimy / 2 + src_num_vram_rows * 6 + dst_num_vram_rows * (20 + 11) + 12;
	blit_delay_ns += num_vram_clk * CV1K_VRAM_CLK_NANOSEC;
}

static void gfx_draw(UINT32 *addr)
{
	int tint_r, tint_g, tint_b, tinted = 0;
	const UINT16 attr = read_next_word(addr);
	const UINT16 alpha = read_next_word(addr);
	UINT16 src_x = read_next_word(addr);
	UINT16 src_y = read_next_word(addr);
	const UINT16 dst_x_start = read_next_word(addr);
	const UINT16 dst_y_start = read_next_word(addr);
	const UINT16 w = read_next_word(addr);
	const UINT16 h = read_next_word(addr);
	const UINT16 tint_rw = read_next_word(addr);
	const UINT16 tint_gb = read_next_word(addr);

	const int d_mode = attr & 0x0007;
	const int s_mode = (attr & 0x0070) >> 4;
	const int trans = (attr >> 8) & 1;
	int blend = (attr >> 9) & 1;
	const int flipy = (attr >> 10) & 1;
	const int flipx = (attr >> 11) & 1;
	const int d_alpha = (alpha & 0x00ff) >> 3;
	const int s_alpha = ((alpha & 0xff00) >> 8) >> 3;
	const int x = (INT16)dst_x_start;
	const int y = (INT16)dst_y_start;
	const int dimx = (w & 0x1fff) + 1;
	const int dimy = (h & 0x0fff) + 1;

	src_x &= 0x1fff;
	src_y &= 0x0fff;

	/* 0x20 is the 'normal' tint, not 0x1f */
	tint_r = (tint_rw & 0xff) >> 2;
	tint_g = ((tint_gb >> 8) & 0xff) >> 2;
	tint_b = (tint_gb & 0xff) >> 2;
	if (tint_r != 0x20 || tint_g != 0x20 || tint_b != 0x20)
		tinted = 1;

	if ((s_mode == 0 && s_alpha == 0x1f) && (d_mode == 4 && d_alpha == 0x1f))
		blend = 0;

#define DRAW_PARAMS		&draw_clip, src_x, src_y, x, y, dimx, dimy, flipy, s_alpha, d_alpha, tint_r, tint_g, tint_b

	if (tinted)
	{
		if (!flipx)
		{
			if (trans)
			{
				if (!blend) draw_sprite_f0_ti1_tr1_plain(DRAW_PARAMS);
				else blit_funcs_f0_ti1_tr1[s_mode | (d_mode << 3)](DRAW_PARAMS);
			}
			else
			{
				if (!blend) draw_sprite_f0_ti1_tr0_plain(DRAW_PARAMS);
				else blit_funcs_f0_ti1_tr0[s_mode | (d_mode << 3)](DRAW_PARAMS);
			}
		}
		else
		{
			if (trans)
			{
				if (!blend) draw_sprite_f1_ti1_tr1_plain(DRAW_PARAMS);
				else blit_funcs_f1_ti1_tr1[s_mode | (d_mode << 3)](DRAW_PARAMS);
			}
			else
			{
				if (!blend) draw_sprite_f1_ti1_tr0_plain(DRAW_PARAMS);
				else blit_funcs_f1_ti1_tr0[s_mode | (d_mode << 3)](DRAW_PARAMS);
			}
		}
	}
	else if (!blend)
	{
		if (!flipx)
		{
			if (trans) draw_sprite_f0_ti0_tr1_simple(DRAW_PARAMS);
			else draw_sprite_f0_ti0_tr0_simple(DRAW_PARAMS);
		}
		else
		{
			if (trans) draw_sprite_f1_ti0_tr1_simple(DRAW_PARAMS);
			else draw_sprite_f1_ti0_tr0_simple(DRAW_PARAMS);
		}
	}
	else
	{
		if (!flipx)
		{
			if (trans) blit_funcs_f0_ti0_tr1[s_mode | (d_mode << 3)](DRAW_PARAMS);
			else blit_funcs_f0_ti0_tr0[s_mode | (d_mode << 3)](DRAW_PARAMS);
		}
		else
		{
			if (trans) blit_funcs_f1_ti0_tr1[s_mode | (d_mode << 3)](DRAW_PARAMS);
			else blit_funcs_f1_ti0_tr0[s_mode | (d_mode << 3)](DRAW_PARAMS);
		}
	}
#undef DRAW_PARAMS
}

/* the timing pass (gfx_create_shadow_copy of MAME) */
static void gfx_create_timing(void)
{
	UINT32 addr = gfx_addr & 0x1fffffff;

	set_clip(1);
	for (;;)
	{
		const UINT16 data = read_next_word(&addr);

		switch (data & 0xf000)
		{
			case 0x0000:
			case 0xf000:
				return;

			case 0xc000:
				set_clip(read_next_word(&addr));
				idle_blitter(CV1K_CLIP_OPERATION_SIZE_BYTES);
				break;

			case 0x2000:
				addr -= 2;
				gfx_upload_timing(&addr);
				break;

			case 0x1000:
				addr -= 2;
				gfx_draw_timing(&addr);
				break;

			default:
				return;
		}
	}
}

static void gfx_exec(void)
{
	UINT32 addr = gfx_addr_shadowcopy & 0x1fffffff;

	set_clip(1);
	for (;;)
	{
		const UINT16 data = read_next_word(&addr);

		switch (data & 0xf000)
		{
			case 0x0000:
			case 0xf000:
				return;

			case 0xc000:
				set_clip(read_next_word(&addr));
				break;

			case 0x2000:
				addr -= 2;
				gfx_upload(&addr);
				break;

			case 0x1000:
				addr -= 2;
				gfx_draw(&addr);
				break;

			default:
				return;
		}
	}
}

static void blitter_delay_callback(int param)
{
	blitter_busy = 0;
}

static void gfx_exec_w(data32_t data, data32_t mem_mask)
{
	if (ACCESSING_LSB32 && (data & 1))
	{
		gfx_clip_x_shadowcopy = gfx_clip_x;
		gfx_clip_y_shadowcopy = gfx_clip_y;

		/* the time the blit list takes */
		blit_delay_ns = 0;
		gfx_create_timing();

		/* every 63.6 us the blitter fetches a line of VRAM for the screen */
		blit_delay_ns += (blit_delay_ns / CV1K_VRAM_H_LINE_PERIOD_NANOSEC) * CV1K_VRAM_H_LINE_DURATION_NANOSEC;

		blitter_busy = 1;
		timer_adjust(blitter_delay_timer, TIME_IN_NSEC(blit_delay_ns), 0, 0);

		gfx_addr_shadowcopy = gfx_addr;
		gfx_exec();
	}
}


/***************************************************************************
	registers
***************************************************************************/

READ32_HANDLER( cv1k_blitter_r )
{
	sh3_update_access_cycles(0xb8000000 + offset, 0);
	switch (offset * 4)
	{
		case 0x10:
			return blitter_busy ? 0x00000000 : 0x00000010;
		case 0x24:
		case 0x28:
			return 0xffffffff;
		case 0x50:
			return readinputport(0);
	}
	return 0;
}

WRITE32_HANDLER( cv1k_blitter_w )
{
	sh3_update_access_cycles(0xb8000000 + offset, 1);
	switch (offset * 4)
	{
		case 0x04:	gfx_exec_w(data, mem_mask); break;
		case 0x08:	COMBINE_DATA(&gfx_addr); break;
		case 0x14:	COMBINE_DATA(&gfx_scroll_x); break;
		case 0x18:	COMBINE_DATA(&gfx_scroll_y); break;
		case 0x40:	COMBINE_DATA(&gfx_clip_x); break;
		case 0x44:	COMBINE_DATA(&gfx_clip_y); break;
		/* 0x24: a handshake at the start of the IRQs; 0x28: coins; 0x30,
		   0x34: contrast and brightness, 0x38, 0x3c: V and H offsets
		   (test menu) */
		default:	break;
	}
}

/* the FPGA firmware, loaded through port J at boot: only its checksum
   tells the version (A: 03, B: 3e, C: f9, D: e1), nothing uses it */
data8_t cv1k_fpga_r(void)
{
	return 0xff;
}

void cv1k_fpga_w(data8_t data)
{
	/* 0x08 CE, 0x10 CLK, 0x20 DATA */
	if ((data & 0x08) && !(firmware_port & 0x10) && (data & 0x10))
	{
		if (firmware_pos < 2323240 && (data & 0x20))
			firmware_checksum += 1 << (firmware_pos & 7);
		firmware_pos++;
	}
	firmware_port = data;
}


/***************************************************************************
	video
***************************************************************************/

void cv1k_blitter_reset(void)
{
	blitter_busy = 0;
	timer_adjust(blitter_delay_timer, TIME_NEVER, 0, 0);
}

VIDEO_START( cv1k )
{
	int x, y;

	/* padding after the VRAM: a source can run past the end of its row */
	cv1k_vram = auto_malloc((VRAM_W * VRAM_H + 2 * VRAM_W) * sizeof(UINT16));
	if (!cv1k_vram)
		return 1;
	memset(cv1k_vram, 0, (VRAM_W * VRAM_H + 2 * VRAM_W) * sizeof(UINT16));

	/* tables to avoid divides in the blits, pre-clamped */
	for (y = 0; y < 0x40; y++)
		for (x = 0; x < 0x20; x++)
		{
			int v = (x * y) / 0x1f;
			colrtable[x][y] = v > 0x1f ? 0x1f : v;
			colrtable_rev[x ^ 0x1f][y] = v > 0x1f ? 0x1f : v;
		}
	for (y = 0; y < 0x20; y++)
		for (x = 0; x < 0x20; x++)
			colrtable_add[x][y] = (x + y) > 0x1f ? 0x1f : (x + y);

	blitter_delay_timer = timer_alloc(blitter_delay_callback);

	gfx_addr = gfx_scroll_x = gfx_scroll_y = gfx_clip_x = gfx_clip_y = 0;
	gfx_addr_shadowcopy = gfx_clip_x_shadowcopy = gfx_clip_y_shadowcopy = 0;
	blitter_busy = 0;
	blit_delay_ns = 0;
	blit_idle_op_bytes = 0;
	firmware_pos = 0;
	firmware_port = 0;
	firmware_checksum = 0;
	set_clip(0);

	state_save_register_UINT16("cv1k_blit", 0, "vram", cv1k_vram, VRAM_W * VRAM_H);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_addr", &gfx_addr, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_scroll_x", &gfx_scroll_x, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_scroll_y", &gfx_scroll_y, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_clip_x", &gfx_clip_x, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_clip_y", &gfx_clip_y, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_addr_shadow", &gfx_addr_shadowcopy, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_clip_x_shadow", &gfx_clip_x_shadowcopy, 1);
	state_save_register_UINT32("cv1k_blit", 0, "gfx_clip_y_shadow", &gfx_clip_y_shadowcopy, 1);
	state_save_register_UINT8("cv1k_blit", 0, "busy", &blitter_busy, 1);
	state_save_register_UINT32("cv1k_blit", 0, "delay_ns", &blit_delay_ns, 1);
	state_save_register_UINT16("cv1k_blit", 0, "idle_op_bytes", &blit_idle_op_bytes, 1);
	state_save_register_INT32("cv1k_blit", 0, "firmware_pos", &firmware_pos, 1);
	state_save_register_UINT8("cv1k_blit", 0, "firmware_port", &firmware_port, 1);
	state_save_register_UINT8("cv1k_blit", 0, "firmware_checksum", &firmware_checksum, 1);
	return 0;
}

/* the frame: the VRAM scrolled to the screen (copyscrollbitmap of MAME) */
VIDEO_UPDATE( cv1k )
{
	int sx = gfx_scroll_x & (VRAM_W - 1);
	int sy = gfx_scroll_y & (VRAM_H - 1);
	int x, y;
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		const UINT16 *src = &cv1k_vram[((y + sy) & (VRAM_H - 1)) * VRAM_W];
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
			dst[x] = src[(x + sx) & (VRAM_W - 1)] & 0x7fff;
	}
}
