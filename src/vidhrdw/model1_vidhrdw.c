/***************************************************************************

    Sega Model 1 video: TGP display list interpreter, flat-shaded
    polygon rasterizer and System 24 tilemaps.

    license:BSD-3-Clause
    copyright-holders:Olivier Galibert

    Ported to C for MAME 2003-plus from current MAME
    (src/mame/sega/model1_v.cpp).  Behaviour follows current MAME; the
    differences are only in the plumbing (0.78 bitmaps/palette/tilemaps,
    vblank hook, no glm, no logging).

***************************************************************************/

#include <math.h>

/* no fused multiply-add: the TGP rounds every product and sum, and x86 (the
   reference) does not fuse them either. GCC and clang fuse a*b+c by default
   on CPUs that have the instruction (PowerPC, ARM64) */
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "driver.h"
#include "state.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/segaic24.h"
#include "includes/model1.h"


/* Model 1 geometrizer TGP and rasterizer simulation */
#define FRAC_SHIFT	16
#define MOIRE		0x01000000

/* std::min / std::max with the exact same NaN behaviour */
#define STD_MIN(a, b)	(((b) < (a)) ? (b) : (a))
#define STD_MAX(a, b)	(((a) < (b)) ? (b) : (a))

enum { RENDER_BELOW_HUD = 0, RENDER_ABOVE_HUD = 1 };

UINT16 *model1_display_list0, *model1_display_list1;
UINT16 *model1_color_xlat;


/*************************************
 *  Types
 *************************************/

struct m1_vec3
{
	float x, y, z;
};

struct m1_spoint
{
	INT32 x, y;
};

struct m1_point
{
	float x, y, z;
	float xx, yy;
	struct m1_spoint s;
};

struct m1_quad
{
	struct m1_point *p[4];
	float z;
	int col;
};

struct m1_lightparam
{
	float a;
	float d;
	float s;
	int p;
};

struct m1_view
{
	int xc, yc, x1, y1, x2, y2;
	float zoomx, zoomy, viewx, viewy;
	float a_bottom, a_top, a_left, a_right;
	float vxx, vyy, vzz, ayy, ayyc, ayys;
	float translation[12];
	struct m1_vec3 light;
	int spec_enable;
	struct m1_lightparam lightparams[256];
};

struct m1_clipper
{
	int  (*isclipped)(struct m1_view *view, struct m1_point *p);
	void (*clip)(struct m1_view *view, struct m1_point *pt, struct m1_point *p1, struct m1_point *p2);
};


/*************************************
 *  State
 *************************************/

static struct m1_view m1view;
static struct m1_view *m_view = &m1view;

static struct m1_point *m_pointdb, *m_pointpt;
static struct m1_quad *m_quaddb, *m_quadpt;
static struct m1_quad **m_quadind;

static UINT32 *m_poly_rom, *m_poly_ram;
static UINT16 *m_tgp_ram;
static UINT16 m_listctl[2];
static UINT16 *m_display_list_current;
static int m_render_done;
static int m_above_hud;		/* the display list has objects drawn above the HUD */

static struct m1_clipper m_clipfn[4];

static int m_overlay_stride;
static UINT8 *m_overlay_block;
static UINT32 *m_overlay_hud_snapshot;

/* frame number of the last vblank-start processing (see model1_video_vblank_start) */
static int m_vblank_frame;


/*************************************
 *  Helpers
 *************************************/

static INLINE float u2f(UINT32 v)
{
	union { float f; UINT32 u; } x;
	x.u = v;
	return x.f;
}

/* float to int as x86 converts it (MAME's reference): NaN and values out
   of range give INT_MIN. C leaves them undefined and other CPUs (PowerPC,
   ARM) saturate, which drew stray edge pixels from far away vertices */
static int f2i(double v)
{
	if (!(v > -2147483649.0 && v < 2147483648.0))
		return INT_MIN;
	return (int)v;
}

/* std::lround replacement (round half away from zero) */
static int m1_lround(double v)
{
	double r;
	if (v >= 0.0)
	{
		r = floor(v);
		if (v - r >= 0.5)
			r += 1.0;
	}
	else
	{
		r = ceil(v);
		if (r - v >= 0.5)
			r -= 1.0;
	}
	return (int)(long)r;
}

static INLINE float vec3_dot(const struct m1_vec3 *a, const struct m1_vec3 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

/* glm::normalize: v * inversesqrt(dot(v, v)) */
static INLINE void vec3_normalize(struct m1_vec3 *v)
{
	float inv = 1.0f / (float)sqrt((double)vec3_dot(v, v));
	v->x *= inv;
	v->y *= inv;
	v->z *= inv;
}

/* position of the channels in a direct pen of the screen bitmap, from
   direct_rgb_components (set once per frame, see pen_layout) */
static int m_pen_shift[3];
static UINT32 m_pen_max;	/* 0xff (32 bit) or 0x1f (15 bit) */

static void pen_layout(void)
{
	int i;
	m_pen_max = (Machine->color_depth == 32) ? 0xff : 0x1f;
	for (i = 0; i < 3; i++)
	{
		UINT32 c = direct_rgb_components[i];
		int sh = 0;
		while (c && !(c & 1))
		{
			c >>= 1;
			sh++;
		}
		m_pen_shift[i] = sh;
	}
}

/* xRGB 8:8:8 -> native direct pen of the screen bitmap */
static UINT32 rgb_to_pen(UINT32 rgb)
{
	UINT32 r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
	if (m_pen_max != 0xff)
	{
		r >>= 3;
		g >>= 3;
		b >>= 3;
	}
	return (r << m_pen_shift[0]) | (g << m_pen_shift[1]) | (b << m_pen_shift[2]);
}

static INLINE UINT32 get_pixel(struct mame_bitmap *bitmap, int x, int y)
{
	if (bitmap->depth == 32)
		return ((UINT32 *)bitmap->line[y])[x];
	return ((UINT16 *)bitmap->line[y])[x];
}

static INLINE void set_pixel(struct mame_bitmap *bitmap, int x, int y, UINT32 pen)
{
	if (bitmap->depth == 32)
		((UINT32 *)bitmap->line[y])[x] = pen;
	else
		((UINT16 *)bitmap->line[y])[x] = (UINT16)pen;
}


/*************************************
 *  Display list access
 *************************************/

static UINT32 readi(int adr)
{
	return (UINT32)m_display_list_current[(adr + 0) & 0x7fff] | ((UINT32)m_display_list_current[(adr + 1) & 0x7fff] << 16);
}

static INT16 readi16(int adr)
{
	return (INT16)m_display_list_current[(adr + 0) & 0x7fff];
}

static float readf(int adr)
{
	return u2f(readi(adr));
}


/*************************************
 *  View
 *************************************/

static void transform_point(const struct m1_view *v, struct m1_point *p)
{
	struct m1_point q = *p;
	float xx, zz;
	xx   = v->translation[0] * q.x + v->translation[3] * q.y + v->translation[6] * q.z + v->translation[ 9] + v->vxx;
	p->y = v->translation[1] * q.x + v->translation[4] * q.y + v->translation[7] * q.z + v->translation[10] + v->vyy;
	zz   = v->translation[2] * q.x + v->translation[5] * q.y + v->translation[8] * q.z + v->translation[11] + v->vzz;
	p->x = v->ayyc * xx - v->ayys * zz;
	p->z = v->ayys * xx + v->ayyc * zz;
}

static void transform_vector(const struct m1_view *v, struct m1_vec3 *p)
{
	struct m1_vec3 q = *p;
	struct m1_vec3 row1, row2, row3;
	row1.x = v->translation[0]; row1.y = v->translation[3]; row1.z = v->translation[6];
	row2.x = v->translation[1]; row2.y = v->translation[4]; row2.z = v->translation[7];
	row3.x = v->translation[2]; row3.y = v->translation[5]; row3.z = v->translation[8];
	p->x = vec3_dot(&q, &row1);
	p->y = vec3_dot(&q, &row2);
	p->z = vec3_dot(&q, &row3);
}

static float view_determinant(const struct m1_point *p1, const struct m1_point *p2, const struct m1_point *p3)
{
	float x1 = p2->x - p1->x;
	float y1 = p2->y - p1->y;
	float z1 = p2->z - p1->z;
	float x2 = p3->x - p1->x;
	float y2 = p3->y - p1->y;
	float z2 = p3->z - p1->z;

	return p1->x * (y1 * z2 - y2 * z1) + p1->y * (z1 * x2 - z2 * x1) + p1->z * (x1 * y2 - x2 * y1);
}

static void project_point(const struct m1_view *v, struct m1_point *p)
{
	p->xx = p->x / p->z;
	p->yy = p->y / p->z;
	p->s.x = f2i(v->xc + (p->xx * v->zoomx + v->viewx));
	p->s.y = f2i(v->yc - (p->yy * v->zoomy + v->viewy));
}

static void project_point_direct(const struct m1_view *v, struct m1_point *p)
{
	p->xx = p->x /*/ p->z*/;
	p->yy = p->y /*/ p->z*/;
	p->s.x = f2i(v->xc + p->xx);
	p->s.y = f2i(v->yc - p->yy);
}

/*
 xe = xc + (x/z * zm + tx)
 xe < x1 => xc + (x/z * zm + tx) < x1
         => x/z < (x1-xc-tx)/zm
         => x < z*(x1-xc-tx)/zm

 ye = yc - (y/z * zm + ty)
 ye < y1 => yc - (y/z * zm + ty) < y1
         => -y/z < (y1-yc+ty)/zm
         => y > -z*(y1-yc+ty)/zm

 xf = zf*a
 xx = x1*t+x2(1-t); zz = z1*t+z2(1-t)
 => x1*t+x2(1-t) = z1*t*a+z2*(1-t)*a
 => t*(x1-x2+a*(z2-z1) = -x2+a*z2
 => t = (z2*a-x2)/((z2-z1)*a-(x2-x1))
*/
static void recompute_frustum(struct m1_view *v)
{
	v->a_left   = ( v->x1 - v->xc - v->viewx) / v->zoomx;
	v->a_right  = ( v->x2 - v->xc - v->viewx) / v->zoomx;
	v->a_bottom = (-v->y1 + v->yc - v->viewy) / v->zoomy;
	v->a_top    = (-v->y2 + v->yc - v->viewy) / v->zoomy;
}

static void init_translation_matrix(struct m1_view *v)
{
	memset(v->translation, 0, sizeof(v->translation));
	v->translation[0] = 1.0;
	v->translation[4] = 1.0;
	v->translation[8] = 1.0;
}

static void set_viewport(struct m1_view *v, float xcenter, float ycenter, float xl, float xr, float yb, float yt)
{
	v->xc = xcenter;
	v->yc = ycenter;
	v->x1 = xl;
	v->x2 = xr;
	v->y1 = yb;
	v->y2 = yt;

	recompute_frustum(v);
}

static void set_lightparam(struct m1_view *v, int index, float diffuse, float ambient, float specular, int power)
{
	v->lightparams[index].d = diffuse;
	v->lightparams[index].a = ambient;
	v->lightparams[index].s = specular;
	v->lightparams[index].p = power;
}

static void set_zoom(struct m1_view *v, float x, float y)
{
	v->zoomx = x;
	v->zoomy = y;

	recompute_frustum(v);
}

static void set_light_direction(struct m1_view *v, float x, float y, float z)
{
	v->light.x = x;
	v->light.y = y;
	v->light.z = z;
	vec3_normalize(&v->light);
}

static void set_translation_matrix(struct m1_view *v, const float *mat)
{
	int i;
	for (i = 0; i < 12; i++)
		v->translation[i] = mat[i];
}

static void set_view_translation(struct m1_view *v, float x, float y)
{
	v->viewx = x;
	v->viewy = y;

	recompute_frustum(v);
}


/*************************************
 *  Rasterizer
 *************************************/

static void draw_hline(struct mame_bitmap *bitmap, int x1, int x2, int y, int color)
{
	UINT32 pen = color & ~MOIRE;
	if (bitmap->depth == 32)
	{
		UINT32 *base = (UINT32 *)bitmap->line[y];
		while (x1 <= x2)
		{
			base[x1] = pen;
			x1++;
		}
	}
	else
	{
		UINT16 *base = (UINT16 *)bitmap->line[y];
		while (x1 <= x2)
		{
			base[x1] = (UINT16)pen;
			x1++;
		}
	}
}

static void draw_hline_moired(struct mame_bitmap *bitmap, int x1, int x2, int y, int color)
{
	UINT32 pen = color & ~MOIRE;
	if (bitmap->depth == 32)
	{
		UINT32 *base = (UINT32 *)bitmap->line[y];
		while (x1 <= x2)
		{
			if (!((x1 ^ y) & 1))
				base[x1] = pen;
			x1++;
		}
	}
	else
	{
		UINT16 *base = (UINT16 *)bitmap->line[y];
		while (x1 <= x2)
		{
			if (!((x1 ^ y) & 1))
				base[x1] = (UINT16)pen;
			x1++;
		}
	}
}

static void fill_slope(struct mame_bitmap *bitmap, struct m1_view *view, int color, INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 y1, INT32 y2, INT32 *nx1, INT32 *nx2)
{
	if (y1 > view->y2)
		return;

	if (y2 <= view->y1)
	{
		int delta = y2 - y1;
		*nx1 = x1 + delta * sl1;
		*nx2 = x2 + delta * sl2;
		return;
	}

	if (y2 > view->y2)
		y2 = view->y2 + 1;

	if (y1 < view->y1)
	{
		int delta = view->y1 - y1;
		x1 += delta * sl1;
		x2 += delta * sl2;
		y1 = view->y1;
	}

	if (x1 > x2 || (x1 == x2 && sl1 > sl2))
	{
		INT32 t;
		INT32 *tp;

		t = x1;
		x1 = x2;
		x2 = t;

		t = sl1;
		sl1 = sl2;
		sl2 = t;

		tp = nx1;
		nx1 = nx2;
		nx2 = tp;
	}

	while (y1 < y2)
	{
		if (y1 >= view->y1)
		{
			int xx1 = x1 >> FRAC_SHIFT;
			int xx2 = x2 >> FRAC_SHIFT;
			if (xx1 <= view->x2 || xx2 >= view->x1)
			{
				if (xx1 < view->x1)
					xx1 = view->x1;
				if (xx2 > view->x2)
					xx2 = view->x2;

				if (color & MOIRE)
					draw_hline_moired(bitmap, xx1, xx2, y1, color);
				else
					draw_hline(bitmap, xx1, xx2, y1, color);
			}
		}

		x1 += sl1;
		x2 += sl2;
		y1++;
	}
	*nx1 = x1;
	*nx2 = x2;
}

static void fill_line(struct mame_bitmap *bitmap, struct m1_view *view, int color, INT32 y, INT32 x1, INT32 x2)
{
	int xx1 = x1 >> FRAC_SHIFT;
	int xx2 = x2 >> FRAC_SHIFT;

	if (y > view->y2 || y < view->y1)
		return;

	if (xx1 <= view->x2 || xx2 >= view->x1)
	{
		if (xx1 < view->x1)
			xx1 = view->x1;
		if (xx2 > view->x2)
			xx2 = view->x2;

		if (color & MOIRE)
			draw_hline_moired(bitmap, xx1, xx2, y, color);
		else
			draw_hline(bitmap, xx1, xx2, y, color);
	}
}

/*
  Draw a solid line between two screen points (integer Bresenham).  Wireframe
  primitives reach the rasterizer as a degenerate quad with two coincident
  vertex pairs (A,A,B,B); the scanline filler only touches one pixel per row,
  collapsing near-horizontal wires to dots (e.g. the Star Wars Arcade target
  box).  The endpoints come from the unclipped projection and can be garbage
  (a degenerate projection yields inf/NaN, which float->int conversion turns
  into INT32_MIN on x86 hosts), so clip the segment to the viewport first,
  like fill_slope() does for the filler - unclipped, wingwar hung during boot
  walking a ~2^31-pixel line.
*/
static void draw_wireframe_line(struct mame_bitmap *bitmap, int cleft, int cright, int ctop, int cbottom, int x1, int y1, int x2, int y2, UINT32 color, int moire)
{
	/* Liang-Barsky clip against the viewport rectangle (doubles hold every
	   int32 exactly, so in-range endpoints pass through unchanged) */
	double lx1, ly1, cdx, cdy;
	double p[4], q[4];
	double t0 = 0.0, t1 = 1.0;
	int i, dx, dy, sx, sy, err;

	lx1 = x1;
	ly1 = y1;
	cdx = (double)x2 - lx1;
	cdy = (double)y2 - ly1;
	p[0] = -cdx; p[1] = cdx; p[2] = -cdy; p[3] = cdy;
	q[0] = lx1 - cleft; q[1] = cright - lx1; q[2] = ly1 - ctop; q[3] = cbottom - ly1;

	for (i = 0; i < 4; i++)
	{
		if (p[i] == 0.0)
		{
			if (q[i] < 0.0)
				return; /* parallel to this edge and outside it */
		}
		else
		{
			double t = q[i] / p[i];
			if (p[i] < 0.0)
				t0 = STD_MAX(t0, t);
			else
				t1 = STD_MIN(t1, t);
		}
	}
	if (t0 > t1)
		return; /* entirely outside the viewport */

	if (t1 < 1.0)
	{
		x2 = m1_lround(lx1 + t1 * cdx);
		y2 = m1_lround(ly1 + t1 * cdy);
	}
	if (t0 > 0.0)
	{
		x1 = m1_lround(lx1 + t0 * cdx);
		y1 = m1_lround(ly1 + t0 * cdy);
	}

	dx = abs(x2 - x1);
	dy = abs(y2 - y1);
	sx = x1 < x2 ? 1 : -1;
	sy = y1 < y2 ? 1 : -1;
	err = dx - dy;
	for (;;)
	{
		int e2;
		if (x1 >= 0 && x1 < bitmap->width && y1 >= 0 && y1 < bitmap->height)
			if (!moire || !((x1 ^ y1) & 1))
				set_pixel(bitmap, x1, y1, color);
		if (x1 == x2 && y1 == y2)
			break;
		e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x1 += sx; }
		if (e2 < dx)  { err += dx; y1 += sy; }
	}
}

static void fill_quad(struct mame_bitmap *bitmap, struct m1_view *view, const struct m1_quad *q)
{
	struct m1_spoint p[8];
	int color = q->col;
	int i, pmin, pmax, ps1, ps2;
	INT32 cury, limy, x1, x2;
	INT32 sl1 = 0, sl2 = 0;

	if (color < 0)
		color = -1 - color;

	/* MAME draws xRGB into a 32-bit bitmap; convert to this core's direct pen,
	   keeping the moire flag */
	color = (int)(rgb_to_pen(color & 0xffffff) | (color & MOIRE));

	/* A wireframe primitive arrives as a degenerate quad with only two distinct
	   screen vertices (A,A,B,B); rasterize it as a line so near-horizontal wires
	   keep their full span instead of collapsing to one pixel per row. */
	{
		int ax = q->p[0]->s.x, ay = q->p[0]->s.y;
		int bx = ax, by = ay, ndist = 1;
		for (i = 1; i < 4; i++)
		{
			int vx = q->p[i]->s.x, vy = q->p[i]->s.y;
			if (vx == ax && vy == ay)
				continue;
			if (ndist == 1)
			{
				bx = vx;
				by = vy;
				ndist = 2;
			}
			else if (vx != bx || vy != by)
			{
				ndist = 3;
				break;
			}
		}
		if (ndist == 2)
		{
			/* clip to the viewport, exactly like the scanline filler below does */
			draw_wireframe_line(bitmap, view->x1, view->x2, view->y1, view->y2, ax, ay, bx, by, color & ~MOIRE, (color & MOIRE) != 0);
			return;
		}
	}

	for (i = 0; i < 4; i++)
	{
		p[i].x = p[i+4].x = q->p[i]->s.x << FRAC_SHIFT;
		p[i].y = p[i+4].y = q->p[i]->s.y;
	}

	pmin = 0;
	pmax = 0;
	for (i = 1; i < 4; i++)
	{
		if (p[i].y < p[pmin].y)
			pmin = i;
		if (p[i].y > p[pmax].y)
			pmax = i;
	}

	cury = p[pmin].y;
	limy = p[pmax].y;

	if (cury == limy)
	{
		x1 = p[0].x;
		x2 = p[0].x;
		for (i = 1; i < 4; i++)
		{
			if (p[i].x < x1)
				x1 = p[i].x;
			if (p[i].x > x2)
				x2 = p[i].x;
		}
		fill_line(bitmap, view, color, cury, x1, x2);
		return;
	}

	if (cury > view->y2)
		return;
	if (limy <= view->y1)
		return;

	if (limy > view->y2)
		limy = view->y2;

	ps1 = pmin + 4;
	ps2 = pmin;

	goto startup;

	for (;;)
	{
		if (p[ps1 - 1].y == p[ps2 + 1].y)
		{
			fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
			cury = p[ps1 - 1].y;
			if (cury >= limy)
				break;
			ps1--;
			ps2++;

		startup:
			while (p[ps1 - 1].y == cury)
				ps1--;
			while (p[ps2 + 1].y == cury)
				ps2++;
			x1 = p[ps1].x;
			x2 = p[ps2].x;
			sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
			sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
		}
		else if (p[ps1 - 1].y < p[ps2 + 1].y)
		{
			fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2);
			cury = p[ps1 - 1].y;
			if (cury >= limy)
				break;
			ps1--;
			while (p[ps1 - 1].y == cury)
				ps1--;
			x1 = p[ps1].x;
			sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
		}
		else
		{
			fill_slope(bitmap, view, color, x1, x2, sl1, sl2, cury, p[ps2 + 1].y, &x1, &x2);
			cury = p[ps2 + 1].y;
			if (cury >= limy)
				break;
			ps2++;
			while (p[ps2 + 1].y == cury)
				ps2++;
			x2 = p[ps2].x;
			sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
		}
	}
	if (cury == limy)
		fill_line(bitmap, view, color, cury, x1, x2);
}


/*************************************
 *  Sorting
 *************************************/

static int quad_compare(const struct m1_quad *self, const struct m1_quad *other)
{
	float z2 = other->z;

	if (self->z < z2)
		return +1;
	if (self->z > z2)
		return -1;

	if (self - other < 0)
		return -1;

	return +1;
}

static int comp_quads(const void *q1, const void *q2)
{
	const struct m1_quad *const *c1 = (const struct m1_quad *const *)q1;
	const struct m1_quad *const *c2 = (const struct m1_quad *const *)q2;

	return quad_compare(*c1, *c2);
}

static void sort_quads(void)
{
	int count = m_quadpt - m_quaddb;
	int i;
	for (i = 0; i < count; i++)
		m_quadind[i] = &m_quaddb[i];
	qsort(m_quadind, count, sizeof(struct m1_quad *), comp_quads);
}

static void draw_quads(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	struct m1_view *view = m_view;
	int count = m_quadpt - m_quaddb;
	int i;

	/* clip to the cliprect */
	int save_x1 = view->x1;
	int save_x2 = view->x2;
	int save_y1 = view->y1;
	int save_y2 = view->y2;
	view->x1 = STD_MAX(view->x1, cliprect->min_x);
	view->x2 = STD_MIN(view->x2, cliprect->max_x);
	view->y1 = STD_MAX(view->y1, cliprect->min_y);
	view->y2 = STD_MIN(view->y2, cliprect->max_y);

	for (i = 0; i < count; i++)
		fill_quad(bitmap, view, m_quadind[i]);

	view->x1 = save_x1;
	view->x2 = save_x2;
	view->y1 = save_y1;
	view->y2 = save_y2;
}


/*************************************
 *  Frustum clipping
 *************************************/

static int fclip_isc_bottom(struct m1_view *view, struct m1_point *p)
{
	return p->y > (p->z * view->a_bottom);
}

static void fclip_clip_bottom(struct m1_view *view, struct m1_point *pt, struct m1_point *p1, struct m1_point *p2)
{
	float t = (p2->z * view->a_bottom - p2->y) / ((p2->z - p1->z) * view->a_bottom - (p2->y - p1->y));
	pt->x = p1->x * t + p2->x * (1 - t);
	pt->y = p1->y * t + p2->y * (1 - t);
	pt->z = p1->z * t + p2->z * (1 - t);
	project_point(view, pt);
}

static int fclip_isc_top(struct m1_view *view, struct m1_point *p)
{
	return p->y < (p->z * view->a_top);
}

static void fclip_clip_top(struct m1_view *view, struct m1_point *pt, struct m1_point *p1, struct m1_point *p2)
{
	float t = (p2->z * view->a_top - p2->y) / ((p2->z - p1->z) * view->a_top - (p2->y - p1->y));
	pt->x = p1->x * t + p2->x * (1 - t);
	pt->y = p1->y * t + p2->y * (1 - t);
	pt->z = p1->z * t + p2->z * (1 - t);
	project_point(view, pt);
}

static int fclip_isc_left(struct m1_view *view, struct m1_point *p)
{
	return p->x < (p->z * view->a_left);
}

static void fclip_clip_left(struct m1_view *view, struct m1_point *pt, struct m1_point *p1, struct m1_point *p2)
{
	float t = (p2->z * view->a_left - p2->x) / ((p2->z - p1->z) * view->a_left - (p2->x - p1->x));
	pt->x = p1->x * t + p2->x * (1 - t);
	pt->y = p1->y * t + p2->y * (1 - t);
	pt->z = p1->z * t + p2->z * (1 - t);
	project_point(view, pt);
}

static int fclip_isc_right(struct m1_view *view, struct m1_point *p)
{
	return p->x > (p->z * view->a_right);
}

static void fclip_clip_right(struct m1_view *view, struct m1_point *pt, struct m1_point *p1, struct m1_point *p2)
{
	float t = (p2->z * view->a_right - p2->x) / ((p2->z - p1->z) * view->a_right - (p2->x - p1->x));
	pt->x = p1->x * t + p2->x * (1 - t);
	pt->y = p1->y * t + p2->y * (1 - t);
	pt->z = p1->z * t + p2->z * (1 - t);
	project_point(view, pt);
}

static void fclip_push_quad(int level, struct m1_quad *q);

static void fclip_push_quad_next(int level, struct m1_quad *q, struct m1_point *p1, struct m1_point *p2, struct m1_point *p3, struct m1_point *p4)
{
	struct m1_quad cquad;
	cquad.col = q->col;
	cquad.z = q->z;
	cquad.p[0] = p1;
	cquad.p[1] = p2;
	cquad.p[2] = p3;
	cquad.p[3] = p4;
	fclip_push_quad(level + 1, &cquad);
}

static void fclip_push_quad(int level, struct m1_quad *q)
{
	struct m1_view *view = m_view;
	int is_out[4], is_out2[4];
	struct m1_point *pt[4];
	struct m1_point *pi1, *pi2;
	int i, j;

	if (level == 4)
	{
		*m_quadpt = *q;
		m_quadpt++;
		return;
	}

	for (i = 0; i < 4; i++)
		is_out[i] = m_clipfn[level].isclipped(view, q->p[i]);

	/* No clipping */
	if (!is_out[0] && !is_out[1] && !is_out[2] && !is_out[3])
	{
		fclip_push_quad(level + 1, q);
		return;
	}

	/* Full clipping */
	if (is_out[0] && is_out[1] && is_out[2] && is_out[3])
		return;

	/* Find n so that point n is clipped and n-1 isn't */
	for (i = 0; i < 4; i++)
		if (is_out[i] && !is_out[(i - 1) & 3])
			break;

	for (j = 0; j < 4; j++)
	{
		pt[j] = q->p[(i + j) & 3];
		is_out2[j] = is_out[(i + j) & 3];
	}

	/* At this point, pt[0] is clipped out and pt[3] isn't.  Test for the 4 possible cases */
	if (is_out2[1])
	{
		if (is_out2[2])
		{
			/* pt 0,1,2 clipped out, one triangle left */
			m_clipfn[level].clip(view, m_pointpt, pt[2], pt[3]);
			pi1 = m_pointpt++;
			m_clipfn[level].clip(view, m_pointpt, pt[3], pt[0]);
			pi2 = m_pointpt++;
			fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
		}
		else
		{
			/* pt 0,1 clipped out, one quad left */
			m_clipfn[level].clip(view, m_pointpt, pt[1], pt[2]);
			pi1 = m_pointpt++;
			m_clipfn[level].clip(view, m_pointpt, pt[3], pt[0]);
			pi2 = m_pointpt++;
			fclip_push_quad_next(level, q, pi1, pt[2], pt[3], pi2);
		}
	}
	else
	{
		if (is_out2[2])
		{
			/* pt 0,2 clipped out, shouldn't happen, two triangles */
			m_clipfn[level].clip(view, m_pointpt, pt[0], pt[1]);
			pi1 = m_pointpt++;
			m_clipfn[level].clip(view, m_pointpt, pt[1], pt[2]);
			pi2 = m_pointpt++;
			fclip_push_quad_next(level, q, pi1, pt[1], pi2, pi2);
			m_clipfn[level].clip(view, m_pointpt, pt[2], pt[3]);
			pi1 = m_pointpt++;
			m_clipfn[level].clip(view, m_pointpt, pt[3], pt[0]);
			pi2 = m_pointpt++;
			fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
		}
		else
		{
			/* pt 0 clipped out, one decagon left, split in quad+tri */
			m_clipfn[level].clip(view, m_pointpt, pt[0], pt[1]);
			pi1 = m_pointpt++;
			m_clipfn[level].clip(view, m_pointpt, pt[3], pt[0]);
			pi2 = m_pointpt++;
			fclip_push_quad_next(level, q, pi1, pt[1], pt[2], pt[3]);
			fclip_push_quad_next(level, q, pt[3], pi2, pi1, pi1);
		}
	}
}

static float min4f(float a, float b, float c, float d)
{
	float m = a;
	if (b < m)
		m = b;
	if (c < m)
		m = c;
	if (d < m)
		m = d;
	return m;
}

static float max4f(float a, float b, float c, float d)
{
	float m = a;
	if (b > m)
		m = b;
	if (c > m)
		m = c;
	if (d > m)
		m = d;
	return m;
}


/*************************************
 *  Lighting
 *************************************/

static float compute_specular(const struct m1_vec3 *normal, const struct m1_vec3 *light, float diffuse, int lmode)
{
	const struct m1_lightparam *lp;
	float s;

	if (!m_view->spec_enable)
		return 0.0f;

	lp = &m_view->lightparams[lmode];
	if (lp->p == 0 || lp->s <= 0.0f)
		return 0.0f;

	/* z component of the reflected light vector, as computed by the Model 2
	   GEO program: R.z = 2*(N.L)*N.z - L.z, raised to a power of two selected
	   by the power field and scaled by the specular scale. */
	s = (2.0f * diffuse * normal->z) - light->z;
	if (s <= 0.0f)
		return 0.0f;
	if (lp->p >= 2)
		s *= s;
	if (lp->p >= 4)
		s *= s;
	if (lp->p >= 7)
		s *= s;
	s = s * lp->s;
	return STD_MIN(s, 1.0f);
}


/*************************************
 *  Objects
 *************************************/

/*
  old_z is the flat-z "reuse" register (poly zmode 0 = keep previous poly's
  sort z). It persists ACROSS objects within a display-list walk: NetMerc's
  garage door wings (all-zmode-0 dynamic objects) must inherit the ~325k z of
  the preceding object so the z~40k wall occludes them (real-hardware
  capture); resetting per object made them sort at z=0 (nearest) and paint
  through the wall.
*/
static void push_object(UINT32 tex_adr, UINT32 poly_adr, UINT32 size, float *old_z)
{
	const UINT32 *poly_data;
	struct m1_point *old_p0, *old_p1;
	UINT32 i;

	/* Protect against bad data when attacking a super destroyer */
	if (tex_adr == 0xffffffff || size >= 0x1000000)
		return;

	if (poly_adr & 0x800000)
		poly_data = m_poly_ram;
	else
		poly_data = m_poly_rom;

	poly_adr &= 0x7fffff;

	if (!size)
		size = 0xffffffff;

	old_p0 = m_pointpt++;
	old_p1 = m_pointpt++;

	old_p0->x = u2f(poly_data[poly_adr + 0]);
	old_p0->y = u2f(poly_data[poly_adr + 1]);
	old_p0->z = u2f(poly_data[poly_adr + 2]);
	old_p1->x = u2f(poly_data[poly_adr + 3]);
	old_p1->y = u2f(poly_data[poly_adr + 4]);
	old_p1->z = u2f(poly_data[poly_adr + 5]);
	transform_point(m_view, old_p0);
	transform_point(m_view, old_p1);

	if (old_p0->z > 0)
		project_point(m_view, old_p0);
	else
		old_p0->s.x = old_p0->s.y = 0;

	if (old_p1->z > 0)
		project_point(m_view, old_p1);
	else
		old_p1->s.x = old_p1->s.y = 0;

	poly_adr += 6;

	for (i = 0; i < size; i++)
	{
		UINT32 flags = poly_data[poly_adr];
		int type, lightmode, link;
		struct m1_point *p0, *p1;
		struct m1_vec3 vn;
		struct m1_quad cquad;

		type = flags & 3;
		if (!type)
			break;

		if (flags & 0x00001000)
			tex_adr++;
		/* Flags bit 22 selects the second light parameter bank. netmerc uploads
		   two banks (0x00-0x0c organic/terrain, 0x80-0x9e metal) and mixes
		   polygons from both within a single display list; the other games
		   never set the bit. In the Model 2 GEO attribute word this same bit
		   is the top bit of the texture parameter index. */
		lightmode = ((flags >> 17) & 15) | ((flags & 0x00400000) ? 0x80 : 0);

		p0 = m_pointpt++;
		p1 = m_pointpt++;

		vn.x = u2f(poly_data[poly_adr + 1]);
		vn.y = u2f(poly_data[poly_adr + 2]);
		vn.z = u2f(poly_data[poly_adr + 3]);
		p0->x = u2f(poly_data[poly_adr + 4]);
		p0->y = u2f(poly_data[poly_adr + 5]);
		p0->z = u2f(poly_data[poly_adr + 6]);
		p1->x = u2f(poly_data[poly_adr + 7]);
		p1->y = u2f(poly_data[poly_adr + 8]);
		p1->z = u2f(poly_data[poly_adr + 9]);

		if (type == 2)
		{
			p1->x = p0->x;
			p1->y = p0->y;
			p1->z = p0->z;
		}

		link = (flags >> 8) & 3;

		transform_vector(m_view, &vn);

		transform_point(m_view, p0);
		transform_point(m_view, p1);
		if (p0->z > 0)
			project_point(m_view, p0);
		else
			p0->s.x = p0->s.y = 0;

		if (p1->z > 0)
			project_point(m_view, p1);
		else
			p1->s.x = p1->s.y = 0;

		if (!link)
			goto next;

		if (!(flags & 0x00004000) && view_determinant(old_p1, old_p0, p0) > 0)
			goto next;

		vec3_normalize(&vn);

		cquad.p[0] = old_p1;
		cquad.p[1] = old_p0;
		cquad.p[2] = p0;
		cquad.p[3] = p1;

		switch ((flags >> 10) & 3)
		{
			case 0:
				cquad.z = *old_z;
				break;
			case 1:
				cquad.z = *old_z = min4f(old_p1->z, old_p0->z, p0->z, p1->z);
				break;
			case 2:
				cquad.z = *old_z = max4f(old_p1->z, old_p0->z, p0->z, p1->z);
				break;
			case 3:
			default:
				cquad.z = 0.0;
				break;
		}

		{
			float dif = vec3_dot(&vn, &m_view->light);
			float spec = compute_specular(&vn, &m_view->light, dif, lightmode);
			float ln = m_view->lightparams[lightmode].a + m_view->lightparams[lightmode].d * STD_MAX(0.0f, dif) + spec;
			int lumval = f2i(255.0f * STD_MIN(1.0f, ln));
			/* bits [11:10] of the tile word select a colour mode: bit 10 marks a
			   flat UI element (radar blips, HUD text) drawn unlit, and mode 01
			   additionally blinks by rotating the BGR channels on alternate frames. */
			UINT16 tex_data = m_tgp_ram[tex_adr - 0x40000];
			int color = paletteram16[0x1000 | (tex_data & 0x3ff)];
			int r = (color >> 0x0) & 0x1f;
			int g = (color >> 0x5) & 0x1f;
			int b = (color >> 0xA) & 0x1f;
			if (((tex_data >> 10) & 3) == 1 && (cpu_getcurrentframe() & 1))
			{
				/* rotate b -> g -> r -> b */
				int tmp = b;
				b = r;
				r = g;
				g = tmp;
			}

			lumval >>= 2; /* there must be a luma translation table somewhere */
			if (lumval > 0x3f)
				lumval = 0x3f;
			else if (lumval < 0)
				lumval = 0;

			/* flat UI element: draw unlit at full intensity instead of shading it */
			if (tex_data & 0x400)
				lumval = 0x3f;

			r = (model1_color_xlat[(r << 8) | lumval | 0x0] >> 3) & 0x1f;
			g = (model1_color_xlat[(g << 8) | lumval | 0x2000] >> 3) & 0x1f;
			b = (model1_color_xlat[(b << 8) | lumval | 0x4000] >> 3) & 0x1f;
			cquad.col = (pal5bit(r) << 16) | (pal5bit(g) << 8) | (pal5bit(b) << 0);
		}

		if (flags & 0x00002000)
			cquad.col |= MOIRE;

		fclip_push_quad(0, &cquad);

	next:
		poly_adr += 10;
		switch (link)
		{
			case 0:
			case 2:
				old_p0 = p0;
				old_p1 = p1;
				break;
			case 1:
				old_p1 = p0;
				break;
			case 3:
				old_p0 = p1;
				break;
		}
	}
}

static int push_direct(int list_offset)
{
	UINT32 tex_adr = readi(list_offset + 2);
	struct m1_point *old_p0, *old_p1;

	old_p0 = m_pointpt++;
	old_p1 = m_pointpt++;

	old_p0->x = readf(list_offset + 2 + 4);
	old_p0->y = readf(list_offset + 2 + 6);
	old_p0->z = readf(list_offset + 2 + 8);
	old_p1->x = readf(list_offset + 2 + 12);
	old_p1->y = readf(list_offset + 2 + 14);
	old_p1->z = readf(list_offset + 2 + 16);

	/* Direct (2D-projected) polys are screen-space: project_point_direct only
	   offsets x/y by the viewport centre and ignores z, so project every vertex
	   unconditionally.  Guarding on z > 0 and snapping behind-"camera" vertices
	   to (0,0) spawns huge spurious triangles radiating from the screen origin
	   (the bogus red cross over the SWA cockpit once direct polys draw on top). */
	project_point_direct(m_view, old_p0);
	project_point_direct(m_view, old_p1);

	list_offset += 18;

	for (;;)
	{
		UINT32 flags = readi(list_offset + 2);
		UINT32 lum;
		int type, link;
		float z;
		struct m1_point *p0, *p1;
		struct m1_quad cquad;

		type = flags & 3;
		if (!type)
			break;

		if (flags & 0x00001000)
			tex_adr++;

		/* list+2+2 is luminosity
		   list+2+4 is 0?
		   list+2+12 is z? */

		p0 = m_pointpt++;
		p1 = m_pointpt++;

		lum = readi(list_offset + 2 + 2);

		z = 0;
		if (type == 2)
		{
			p0->x = readf(list_offset + 2 + 6);
			p0->y = readf(list_offset + 2 + 8);
			p0->z = readf(list_offset + 2 + 10);
			z = p0->z;
			*p1 = *p0;
			list_offset += 12;
		}
		else
		{
			p0->x = readf(list_offset + 2 + 6);
			p0->y = readf(list_offset + 2 + 8);
			p0->z = readf(list_offset + 2 + 10);
			p1->x = readf(list_offset + 2 + 14);
			p1->y = readf(list_offset + 2 + 16);
			p1->z = readf(list_offset + 2 + 18);
			z = readf(list_offset + 2 + 12);
			list_offset += 20;
		}

		link = (flags >> 8) & 3;

		project_point_direct(m_view, p0);
		project_point_direct(m_view, p1);

		if (!link)
			goto next;

		cquad.p[0] = old_p1;
		cquad.p[1] = old_p0;
		cquad.p[2] = p0;
		cquad.p[3] = p1;
		cquad.z = z;
		{
			int lumval = f2i(((float)(lum >> 24)) * 2.0f);
			int color = paletteram16[0x1000 | (m_tgp_ram[tex_adr - 0x40000] & 0x3ff)];
			int r = (color >> 0x0) & 0x1f;
			int g = (color >> 0x5) & 0x1f;
			int b = (color >> 0xA) & 0x1f;
			lumval >>= 2; /* there must be a luma translation table somewhere */
			if (lumval > 0x3f) lumval = 0x3f;
			else if (lumval < 0) lumval = 0;
			r = (model1_color_xlat[(r << 8) | lumval | 0x0] >> 3) & 0x1f;
			g = (model1_color_xlat[(g << 8) | lumval | 0x2000] >> 3) & 0x1f;
			b = (model1_color_xlat[(b << 8) | lumval | 0x4000] >> 3) & 0x1f;
			cquad.col = (pal5bit(r) << 16) | (pal5bit(g) << 8) | (pal5bit(b) << 0);
		}
		if (flags & 0x00002000)
		{
			cquad.col |= MOIRE;
			/* A moire (stipple-translucent) direct poly whose flat colour resolves
			   to the transparent texture pen (index 0x1000 -> black) is a "fade
			   toward backdrop" overlay, not a black fill.  Painting the pen
			   literally gives a black/scene checkerboard (the Star Wars Arcade
			   gunner 2nd-viewport overlay); the arcade alternates backdrop/scene.
			   Paint the backdrop pen (palette[0]) so the stipple reads
			   backdrop/scene. */
			if ((m_tgp_ram[tex_adr - 0x40000] & 0x3ff) == 0)
			{
				UINT8 pr, pg, pb;
				palette_get_color(0, &pr, &pg, &pb);
				cquad.col = MOIRE | (pr << 16) | (pg << 8) | pb;
			}

			/* wingwar's display list carries a constant degenerate moire record
			   (all four vertices on one point) that the flat-quad filler would
			   paint as a stray dot at the viewport centre; not seen on hardware. */
			if (cquad.p[0]->s.x == cquad.p[1]->s.x && cquad.p[1]->s.x == cquad.p[2]->s.x && cquad.p[2]->s.x == cquad.p[3]->s.x &&
				cquad.p[0]->s.y == cquad.p[1]->s.y && cquad.p[1]->s.y == cquad.p[2]->s.y && cquad.p[2]->s.y == cquad.p[3]->s.y)
				goto next;
		}

		/* Direct polys are screen-space - project_point_direct only offsets x/y by
		   the viewport centre and ignores z.  fclip_push_quad's frustum clip tests
		   vertices against z-scaled planes (y > z*a_bottom, ...), which for a direct
		   poly with small/zero z collapses every plane onto the origin and culls the
		   whole poly (e.g. the SWA FIGHTER CONTROL console moire rect, all z = 0).
		   Skip the frustum clip and queue the quad directly; fill_quad still scissors
		   it to the viewport rectangle. */
		*m_quadpt++ = cquad;

	next:
		switch (link)
		{
		case 0:
		case 2:
			old_p0 = p0;
			old_p1 = p1;
			break;
		case 1:
			old_p1 = p0;
			break;
		case 3:
			old_p0 = p1;
			break;
		}
	}
	list_offset += 4;
	return list_offset;
}

static int skip_direct(int list_offset)
{
	list_offset += 18;

	for (;;)
	{
		UINT32 flags = readi(list_offset + 2);

		int type = flags & 3;
		if (!type)
			break;

		if (type == 2)
			list_offset += 12;
		else
			list_offset += 20;
	}
	list_offset += 4;
	return list_offset;
}

static void draw_objects(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	if (m_quadpt != m_quaddb)
	{
		sort_quads();
		draw_quads(bitmap, cliprect);
	}

	m_quadpt = m_quaddb;
	m_pointpt = m_pointdb;
}

/* draw_direct() / unsort_quads() are unused in current MAME (direct polys go
   through push_direct in the below-HUD pass); not ported. */


/*************************************
 *  List control
 *************************************/

static void set_current_render_list(void)
{
	if (!(m_listctl[0] & 4))
		m_listctl[0] = (m_listctl[0] & ~0x40) | (m_listctl[0] & 8 ? 0x40 : 0);
	m_display_list_current = m_listctl[0] & 0x40 ? model1_display_list1 : model1_display_list0;
}

static void end_frame(void)
{
	if ((m_listctl[0] & 4) && (cpu_getcurrentframe() & 1))
		m_listctl[0] ^= 0x40;
}

READ16_HANDLER( model1_listctl_r )
{
	if (!offset)
		return m_listctl[0] | 0x30;
	else
		return m_listctl[1];
}

WRITE16_HANDLER( model1_listctl_w )
{
	COMBINE_DATA(m_listctl + offset);
}

WRITE16_HANDLER( model1_paletteram_w )
{
	UINT16 v;
	int r, g, b;

	COMBINE_DATA(&paletteram16[offset]);
	v = paletteram16[offset];
	r = pal5bit((v >> 0) & 0x1f);
	g = pal5bit((v >> 5) & 0x1f);
	b = pal5bit((v >> 10) & 0x1f);
	/* Bit 15 is an intensity/shade bit, not unused as the plain xBGR_555 decode
	   assumes.  Almost every entry has it set (full brightness); a handful clear
	   it to render dimmed (e.g. Star Wars Arcade's grey menu backdrop pen 0x7777).
	   Halve the channels when it is clear so those pens stop reading too bright. */
	if (!(v & 0x8000))
	{
		r >>= 1;
		g >>= 1;
		b >>= 1;
	}
	palette_set_color(offset, r, g, b);
}


/*************************************
 *  TGP display list walk
 *************************************/

static void tgp_render(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int pass)
{
	m_render_done = 1;
	if ((m_listctl[1] & 0x1f) == 0x1f)
	{
		float old_z = 0;
		int list_offset = 0;

		set_current_render_list();

		init_translation_matrix(m_view);

		for (;;)
		{
			int type = readi(list_offset + 0);

			switch (type)
			{
			case 0:
				list_offset += 2;
				break;
			case 1:
				/* command 0x01 = object drawn below the cat1 HUD tilemaps */
				if (pass == RENDER_BELOW_HUD)
					push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), &old_z);
				list_offset += 8;
				break;
			case 0x41:
				/* command 0x41 = object drawn above the HUD (e.g. SWA radar blips) */
				m_above_hud = 1;
				if (pass == RENDER_ABOVE_HUD)
					push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), &old_z);
				list_offset += 8;
				break;
			case 2:
				/* direct (2D-projected) polys; push them in the below-HUD pass so
				   they share the per-viewport z-sort with the 0x01 objects */
				if (pass == RENDER_BELOW_HUD)
					list_offset = push_direct(list_offset);
				else
					list_offset = skip_direct(list_offset);
				break;
			case 3:
			{
				float xc, yc, x1, y1, x2, y2;

				draw_objects(bitmap, cliprect);

				xc = readi16(list_offset + 4);
				yc = 383 - (readi16(list_offset + 6) - 39);
				x1 = readi16(list_offset + 8);
				y2 = 383 - (readi16(list_offset + 10) - 39);
				x2 = readi16(list_offset + 12);
				y1 = 383 - (readi16(list_offset + 14) - 39);

				set_viewport(m_view, xc, yc, x1, x2, y1, y2);

				list_offset += 16;
				break;
			}
			case 4:
			{
				int adr = readi(list_offset + 2);
				int len = readi(list_offset + 4) + 1;
				int i;
				for (i = 0; i < len; i++)
					m_tgp_ram[adr - 0x40000 + i] = readi16(list_offset + 6 + 2 * i);
				list_offset += 6 + len * 2;
				break;
			}
			case 5:
			{
				int adr = readi(list_offset + 2);
				int len = readi(list_offset + 4);
				int i;
				for (i = 0; i < len; i++)
					m_poly_ram[adr - 0x800000 + i] = readi(list_offset + 2 * i + 6);
				list_offset += 6 + len * 2;
				break;
			}
			case 6:
			{
				int adr = readi(list_offset + 2);
				int len = readi(list_offset + 4);
				int i;
				for (i = 0; i < len; i++)
				{
					int v = readi(list_offset + 6 + i * 2);
					float diffuse = ((float)(v & 0xff)) / 255.0f;
					float ambient = ((float)((v >> 8) & 0xff)) / 255.0f;
					float specular = ((float)((v >> 16) & 0xff)) / 255.0f;
					int power = (v >> 24) & 0xff;
					set_lightparam(m_view, i + adr, diffuse, ambient, specular, power);
				}
				list_offset += 6 + len * 2;
				break;
			}
			case 7:
				/* Mode word, as in the Model 2 GEO command 7: bit 0 enables the
				   specular term (netmerc toggles it mid-list, on for the enemy
				   mechs and off for terrain/cockpit); bit 1 tracks the display
				   list double buffer parity. */
				m_view->spec_enable = readi(list_offset + 2) & 1;
				list_offset += 4;
				break;
			case 8:
				list_offset += 4;
				break;
			case 9:
				set_zoom(m_view, readf(list_offset + 2) * 4, readf(list_offset + 4) * 4);
				list_offset += 6;
				break;
			case 0xa:
				set_light_direction(m_view, readf(list_offset + 2), readf(list_offset + 4), readf(list_offset + 6));
				list_offset += 8;
				break;
			case 0xb:
			{
				float mat[12];
				int i;
				for (i = 0; i < 12; i++)
					mat[i] = readf(list_offset + 2 + 2 * i);
				set_translation_matrix(m_view, mat);
				list_offset += 26;
				break;
			}
			case 0xc:
				set_view_translation(m_view, readf(list_offset + 2), readf(list_offset + 4));
				list_offset += 6;
				break;
			case 0xf:
				goto end;
			default:
				goto end;
			}
		}
	end:
		draw_objects(bitmap, cliprect);
	}
}

static void tgp_scan(void)
{
	if (!m_render_done && (m_listctl[1] & 0x1f) == 0x1f)
	{
		int list_offset = 0;

		set_current_render_list();
		/* Skip everything but the data uploads */

		for (;;)
		{
			int type = readi(list_offset + 0);
			switch (type)
			{
				case 0:
					list_offset += 2;
					break;
				case 1:
				case 0x41:
					list_offset += 8;
					break;
				case 2:
					list_offset = skip_direct(list_offset);
					break;
				case 3:
					list_offset += 16;
					break;
				case 4:
				{
					int adr = readi(list_offset + 2);
					int len = readi(list_offset + 4) + 1;
					int i;
					for (i = 0; i < len; i++)
						m_tgp_ram[adr - 0x40000 + i] = readi16(list_offset + 6 + 2 * i);
					list_offset += 6 + len * 2;
					break;
				}
				case 5:
				{
					int adr = readi(list_offset + 2);
					int len = readi(list_offset + 4);
					int i;
					for (i = 0; i < len; i++)
						m_poly_ram[adr - 0x800000 + i] = readi(list_offset + 2 * i + 6);
					list_offset += 6 + len * 2;
					break;
				}
				case 6:
				{
					int adr = readi(list_offset + 2);
					int len = readi(list_offset + 4);
					int i;
					for (i = 0; i < len; i++)
					{
						int v = readi(list_offset + 6 + i * 2);
						m_view->lightparams[i + adr].d = ((float)(v & 0xff)) / 255.0f;
						m_view->lightparams[i + adr].a = ((float)((v >> 8) & 0xff)) / 255.0f;
						m_view->lightparams[i + adr].s = ((float)((v >> 16) & 0xff)) / 255.0f;
						m_view->lightparams[i + adr].p = (v >> 24) & 0xff;
					}
					list_offset += 6 + len * 2;
					break;
				}
				case 7:
					list_offset += 4;
					break;
				case 8:
					list_offset += 4;
					break;
				case 9:
					list_offset += 6;
					break;
				case 0xa:
					list_offset += 8;
					break;
				case 0xb:
					list_offset += 26;
					break;
				case 0xc:
					list_offset += 6;
					break;
				case 0xf:
				case -1:
					goto end;
				default:
					goto end;
			}
		}
	end:
		;
	}
	m_render_done = 0;
}


/*************************************
 *  HUD overlay stencil
 *************************************/

/* Snapshot the HUD after the cat1 tilemaps are composited: a pixel is a bright
   "feature" (grid lines, struts) when any channel rises above near-black,
   otherwise it is transparent/screen background where the above-HUD blips show. */
static void build_overlay_mask(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	/* anything darker than 8 (of 255) counts as HUD background: in 5-bit
	   channels (expanded as c << 3 | c >> 2) that is a value above 1 */
	const UINT32 level = (m_pen_max == 0xff) ? 8 : 1;
	const UINT32 mx = m_pen_max;
	const int s0 = m_pen_shift[0], s1 = m_pen_shift[1], s2 = m_pen_shift[2];
	int x, y;
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			UINT32 pix = get_pixel(bitmap, x, y);
			int i = y * m_overlay_stride + x;
			m_overlay_block[i] = ((pix >> s0) & mx) > level || ((pix >> s1) & mx) > level || ((pix >> s2) & mx) > level;
			m_overlay_hud_snapshot[i] = pix;
		}
	}
}

/* Restore the bright HUD features over the above-HUD blip pass, so blips only
   remain where the HUD pixel was transparent / near-black. */
static void apply_overlay_stencil(struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int x, y;
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			int i = y * m_overlay_stride + x;
			if (m_overlay_block[i])
				set_pixel(bitmap, x, y, m_overlay_hud_snapshot[i]);
		}
	}
}


/*************************************
 *  Video start / update / vblank
 *************************************/

/* The view (viewport, zoom, light, light parameters) is set by display
   list commands that some games send only when a scene starts: save it */
static INT32 m_save_view_i[8];
static float m_save_view_f[14 + 12 + 3];
static float m_save_lp_f[256 * 3];
static INT32 m_save_lp_p[256];

static void view_presave(void)
{
	struct m1_view *v = m_view;
	float *f = m_save_view_f;
	int i;

	m_save_view_i[0] = v->xc;
	m_save_view_i[1] = v->yc;
	m_save_view_i[2] = v->x1;
	m_save_view_i[3] = v->y1;
	m_save_view_i[4] = v->x2;
	m_save_view_i[5] = v->y2;
	m_save_view_i[6] = v->spec_enable;
	m_save_view_i[7] = 0;
	*f++ = v->zoomx; *f++ = v->zoomy; *f++ = v->viewx; *f++ = v->viewy;
	*f++ = v->a_bottom; *f++ = v->a_top; *f++ = v->a_left; *f++ = v->a_right;
	*f++ = v->vxx; *f++ = v->vyy; *f++ = v->vzz;
	*f++ = v->ayy; *f++ = v->ayyc; *f++ = v->ayys;
	for (i = 0; i < 12; i++)
		*f++ = v->translation[i];
	*f++ = v->light.x; *f++ = v->light.y; *f++ = v->light.z;
	for (i = 0; i < 256; i++)
	{
		m_save_lp_f[i * 3 + 0] = v->lightparams[i].a;
		m_save_lp_f[i * 3 + 1] = v->lightparams[i].d;
		m_save_lp_f[i * 3 + 2] = v->lightparams[i].s;
		m_save_lp_p[i] = v->lightparams[i].p;
	}
}

static void view_postload(void)
{
	struct m1_view *v = m_view;
	const float *f = m_save_view_f;
	int i;

	v->xc = m_save_view_i[0];
	v->yc = m_save_view_i[1];
	v->x1 = m_save_view_i[2];
	v->y1 = m_save_view_i[3];
	v->x2 = m_save_view_i[4];
	v->y2 = m_save_view_i[5];
	v->spec_enable = m_save_view_i[6];
	v->zoomx = *f++; v->zoomy = *f++; v->viewx = *f++; v->viewy = *f++;
	v->a_bottom = *f++; v->a_top = *f++; v->a_left = *f++; v->a_right = *f++;
	v->vxx = *f++; v->vyy = *f++; v->vzz = *f++;
	v->ayy = *f++; v->ayyc = *f++; v->ayys = *f++;
	for (i = 0; i < 12; i++)
		v->translation[i] = *f++;
	v->light.x = *f++; v->light.y = *f++; v->light.z = *f++;
	for (i = 0; i < 256; i++)
	{
		v->lightparams[i].a = m_save_lp_f[i * 3 + 0];
		v->lightparams[i].d = m_save_lp_f[i * 3 + 1];
		v->lightparams[i].s = m_save_lp_f[i * 3 + 2];
		v->lightparams[i].p = m_save_lp_p[i];
	}
	m_display_list_current = m_listctl[0] & 0x40 ? model1_display_list1 : model1_display_list0;
}

VIDEO_START( model1 )
{
	int overlay_pixels;

	memset(&m1view, 0, sizeof(m1view));
	m_view = &m1view;

	if (sys24_tile_vh_start(0x3fff))
		return 1;

	m_poly_rom = (UINT32 *)memory_region(REGION_USER1);
	m_poly_ram = auto_malloc(0x400000 * sizeof(UINT32));
	m_tgp_ram = auto_malloc((0x100000 - 0x40000) * sizeof(UINT16));
	m_pointdb = auto_malloc(1000000 * 2 * sizeof(struct m1_point));
	m_quaddb  = auto_malloc(1000000 * sizeof(struct m1_quad));
	m_quadind = auto_malloc(1000000 * sizeof(struct m1_quad *));
	m_overlay_stride = Machine->drv->screen_width;
	overlay_pixels = m_overlay_stride * Machine->drv->screen_height;
	m_overlay_block = auto_malloc(overlay_pixels);
	m_overlay_hud_snapshot = auto_malloc(overlay_pixels * sizeof(UINT32));

	if (!m_poly_ram || !m_tgp_ram || !m_pointdb || !m_quaddb || !m_quadind || !m_overlay_block || !m_overlay_hud_snapshot)
		return 1;

	memset(m_poly_ram, 0, 0x400000 * sizeof(UINT32));
	memset(m_tgp_ram, 0, (0x100000 - 0x40000) * sizeof(UINT16));
	memset(m_quadind, 0, 1000000 * sizeof(struct m1_quad *));

	m_pointpt = m_pointdb;
	m_quadpt = m_quaddb;
	m_listctl[0] = m_listctl[1] = 0;
	m_render_done = 0;
	m_vblank_frame = -1;
	m_display_list_current = model1_display_list0;

	m_clipfn[0].isclipped = fclip_isc_bottom;
	m_clipfn[0].clip = fclip_clip_bottom;
	m_clipfn[1].isclipped = fclip_isc_top;
	m_clipfn[1].clip = fclip_clip_top;
	m_clipfn[2].isclipped = fclip_isc_left;
	m_clipfn[2].clip = fclip_clip_left;
	m_clipfn[3].isclipped = fclip_isc_right;
	m_clipfn[3].clip = fclip_clip_right;

	state_save_register_UINT16("model1_video", 0, "tgp_ram", m_tgp_ram, 0x100000 - 0x40000);
	state_save_register_UINT32("model1_video", 0, "poly_ram", m_poly_ram, 0x40000);
	state_save_register_UINT16("model1_video", 0, "listctl", m_listctl, 2);
	state_save_register_INT32("model1_video", 0, "view_i", m_save_view_i, 8);
	state_save_register_float("model1_video", 0, "view_f", m_save_view_f, 14 + 12 + 3);
	state_save_register_float("model1_video", 0, "light_f", m_save_lp_f, 256 * 3);
	state_save_register_INT32("model1_video", 0, "light_p", m_save_lp_p, 256);
	state_save_register_func_presave(view_presave);
	state_save_register_func_postload(view_postload);

	return 0;
}

VIDEO_UPDATE( model1 )
{
	struct m1_view *view = m_view;

	sys24_tile_update();
	pen_layout();

	view->ayyc = (float)cos((double)view->ayy);
	view->ayys = (float)sin((double)view->ayy);

	fillbitmap(priority_bitmap, 0, NULL);
	fillbitmap(bitmap, Machine->pens[0], cliprect);

	/* draw tilemap B as opaque */
	sys24_tile_draw(bitmap, cliprect, 6, 0, TILEMAP_IGNORE_TRANSPARENCY);
	sys24_tile_draw(bitmap, cliprect, 4, 0, TILEMAP_IGNORE_TRANSPARENCY);
	sys24_tile_draw(bitmap, cliprect, 2, 0, 0);
	sys24_tile_draw(bitmap, cliprect, 0, 0, 0);

	/* 3D objects (0x01) and direct polys (0x02) render below the HUD tilemaps,
	   sharing the per-viewport z-sort. */
	m_above_hud = 0;
	tgp_render(bitmap, cliprect, RENDER_BELOW_HUD);

	/* HUD tilemaps (cat1): cockpit struts, grid lines, radar screen background. */
	sys24_tile_draw(bitmap, cliprect, 7, 0, 0);
	sys24_tile_draw(bitmap, cliprect, 5, 0, 0);
	sys24_tile_draw(bitmap, cliprect, 3, 0, 0);
	sys24_tile_draw(bitmap, cliprect, 1, 0, 0);

	/* 0x41 objects (e.g. radar blips) draw on top of the HUD, but the hardware
	   only lets them through "background" HUD pixels. Snapshot the HUD, draw the
	   above-HUD pass, then restore the bright HUD features over it. */
	if (m_above_hud)
	{
		build_overlay_mask(bitmap, cliprect);
		tgp_render(bitmap, cliprect, RENDER_ABOVE_HUD);
		apply_overlay_stencil(bitmap, cliprect);
	}
}

/*
  Current MAME runs screen_vblank_model1 (tgp_scan + end_frame) on the rising
  edge of vblank, i.e. BEFORE the VIDEO_UPDATE_AFTER_VBLANK screen update of
  the same frame number.  The driver should call this from its vblank
  interrupt (which this core runs at vblank start); VIDEO_EOF only falls back
  to it for frames where that did not happen (e.g. V60 halted).
*/
void model1_video_vblank_start(void)
{
	m_vblank_frame = cpu_getcurrentframe();
	tgp_scan();
	end_frame();
}

VIDEO_EOF( model1 )
{
	if (m_vblank_frame != cpu_getcurrentframe())
	{
		m_vblank_frame = cpu_getcurrentframe();
		tgp_scan();
		end_frame();
	}
}
