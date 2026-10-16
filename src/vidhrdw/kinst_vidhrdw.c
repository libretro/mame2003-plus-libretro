/***************************************************************************

	Killer Instinct hardware

	driver by Aaron Giles and Bryan McPhail

***************************************************************************/

#include "driver.h"
#include "kinst.h"


static data32_t *video_base;

/* hardware is xBBBBBGGGGGRRRRR; the direct RGB bitmap is xRRRRRGGGGGBBBBB */
#define KINST_RGB(v) ((((v) & 0x001f) << 10) | ((v) & 0x03e0) | (((v) >> 10) & 0x001f))



/*************************************
 *
 *	Video RAM base selection
 *
 *************************************/

void kinst_set_video_base(data32_t *base)
{
	/* the frame buffer is read straight from RAM, as in current MAME */
	video_base = base;
}



/*************************************
 *
 *	Palette setup
 *
 *************************************/

PALETTE_INIT( kinst )
{
	int i;

	/* standard 5-5-5 RGB palette */
	for (i = 0; i < 32768; i++)
	{
		int r = i & 31;
		int g = (i >> 5) & 31;
		int b = (i >> 10) & 31;
		palette_set_color(i, (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
	}
}



/*************************************
 *
 *	Video system start
 *
 *************************************/

VIDEO_START( kinst )
{
	return 0;
}



/*************************************
 *
 *	Main refresh
 *
 *************************************/

/*
	Two pixels per 32-bit word, swapped from xBBBBBGGGGGRRRRR to
	xRRRRRGGGGGBBBBB. On little-endian hosts a whole word is converted at
	once (both pixels in one go, the same bits as KINST_RGB on each half)
	and stored as one word. The result is
	the same as the per-pixel loop, which the other hosts keep. NEON and SSE2
	do eight pixels at a time.
*/
#if !defined(MSB_FIRST) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#include <arm_neon.h>
#define KINST_NEON 1
#elif !defined(MSB_FIRST) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define KINST_SSE2 1
#endif

static INLINE UINT32 kinst_rgb2(UINT32 pix)
{
	return ((pix & 0x001f001f) << 10) | (pix & 0x03e003e0) | ((pix >> 10) & 0x001f001f);
}

VIDEO_UPDATE( kinst )
{
	int x, y;

	/* loop over rows and copy to the destination */
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const data32_t *src = &video_base[640/4 * y + cliprect->min_x / 2];
		UINT16 *dest = (UINT16 *)bitmap->line[y] + (cliprect->min_x & ~1);
		x = cliprect->min_x & ~1;

#ifndef MSB_FIRST
		if (!((size_t)dest & 3))
		{
			UINT32 *dest32 = (UINT32 *)dest;
#ifdef KINST_NEON
			const uint16x8_t lo5 = vdupq_n_u16(0x001f), mid5 = vdupq_n_u16(0x03e0);
			for ( ; x + 7 <= cliprect->max_x; x += 8)
			{
				uint16x8_t v = vreinterpretq_u16_u32(vld1q_u32(src));
				uint16x8_t r = vorrq_u16(vorrq_u16(vshlq_n_u16(vandq_u16(v, lo5), 10), vandq_u16(v, mid5)),
				                         vandq_u16(vshrq_n_u16(v, 10), lo5));
				vst1q_u32(dest32, vreinterpretq_u32_u16(r));
				src += 4;
				dest32 += 4;
			}
#endif
#ifdef KINST_SSE2
			const __m128i lo5 = _mm_set1_epi16(0x001f), mid5 = _mm_set1_epi16(0x03e0);
			for ( ; x + 7 <= cliprect->max_x; x += 8)
			{
				__m128i v = _mm_loadu_si128((const __m128i *)src);
				__m128i r = _mm_or_si128(_mm_or_si128(_mm_slli_epi16(_mm_and_si128(v, lo5), 10), _mm_and_si128(v, mid5)),
				                         _mm_and_si128(_mm_srli_epi16(v, 10), lo5));
				_mm_storeu_si128((__m128i *)dest32, r);
				src += 4;
				dest32 += 4;
			}
#endif
			for ( ; x <= cliprect->max_x; x += 2)
				*dest32++ = kinst_rgb2(*src++);
			continue;
		}
#endif

		/* two pixels per 32-bit word */
		for ( ; x <= cliprect->max_x; x += 2)
		{
			data32_t pix = *src++;
			*dest++ = KINST_RGB(pix);
			*dest++ = KINST_RGB(pix >> 16);
		}
	}
}
