/***************************************************************************

    Namco System 22 / Super System 22 video hardware

    Backported from current MAME (Phil Stroffolino, hap, R. Belmont and
    others, BSD-3-Clause). The polygon rasterizer follows MAME's poly.h
    (edges, pixel centers and rounding), single threaded.

***************************************************************************/

#include <math.h>
#include "driver.h"
#include "state.h"
#include "includes/namcos22.h"

typedef double poly3d_t;

#define SCREEN_W	640
#define SCREEN_H	480

/***************************************************************************
    State
***************************************************************************/

static UINT32 *pens;					/* 0x8000 xRGB */
static UINT8 *dirtypal;					/* 0x2000 */
static UINT16 *mix_bitmap;				/* 640x480 text layer pens */
static UINT8 *primap;					/* 640x480 */
static UINT32 *frame;					/* 640x480 render target */

static UINT16 *texture_tilemap;			/* 0x100000 */
static UINT8 *texture_tileattr;			/* 0x100000 */
static UINT32 *texture_tile32;			/* 0x100000: (tile << 12) | attr */
static const UINT8 *texture_tiledata;
static UINT8 texture_ayx_to_pixel[16 * 16 * 16];
static const UINT8 *sprite_data;
static int sprite_count;

static int mixer_flags;
static int screen_fade_factor;
static int screen_fade_r, screen_fade_g, screen_fade_b;
static int poly_fade_enabled;
static int poly_fade_r, poly_fade_g, poly_fade_b;
static int poly_alpha_color, poly_alpha_pen, poly_alpha_factor;
static UINT32 fog_colormask;
static int fog_r, fog_g, fog_b;
static UINT8 *recalc_czram[4];
static int fog_r_per_cztype[4], fog_g_per_cztype[4], fog_b_per_cztype[4];
static UINT16 czattr[8];
static UINT16 *banked_czram[4];
static UINT32 cz_was_written[4];
static UINT16 *spotram;
static UINT32 spotram_enable;
static UINT32 spotram_address;
static int chipselect;
static int spot_factor;
static int text_palbase;
static int bg_palbase;

static UINT8 text_row_used[SCREEN_H];	/* rows with opaque text pixels */
static UINT32 gamma_lut[3][256];		/* gamma of the frame: R << 16, G << 8, B */
static UINT16 tilemapattr[8];
static UINT16 rowscroll[480];
static UINT16 lastrow;
static UINT32 rs_frame;
static mame_timer *posirq_timer;

/* slave DSP simulation */
static int absolute_priority;
static int objectshift;
static int cz_adjust;
static int objectflags;
static float viewmatrix[4][4];
static UINT8 reflection;
static int cullflip;
#define MAX_LIT_SURFACES 0x80
static UINT8 lit_surface_info[MAX_LIT_SURFACES];
static unsigned lit_surface_count;
static unsigned lit_surface_index;
static int lit_surface_width;
static int lit_surface_intensity;
static int lit_surface_gouraud;
static int lit_surface_triangles;
static int camera_vx, camera_vy, camera_vu, camera_vd, camera_vl, camera_vr;
static float camera_zoom;
static float camera_lx, camera_ly, camera_lz;
static int camera_ambient;
static int camera_power;


/***************************************************************************
    Scene graph (z sorted, radix tree as MAME)
***************************************************************************/

#define RADIX_BITS		4
#define RADIX_BUCKETS	(1 << RADIX_BITS)
#define RADIX_MASK		(RADIX_BUCKETS - 1)

enum { NODE_NONLEAF, NODE_QUAD, NODE_SPRITE };

typedef struct
{
	float x, y, z;
	int u, v;	/* 0..0xfff */
	int bri;	/* 0..0xff */
} polyvertex;

struct scenenode
{
	int type;
	struct scenenode *next;
	union
	{
		struct
		{
			struct scenenode *next[RADIX_BUCKETS];
		} nonleaf;
		struct
		{
			int vx, vy;
			int vu, vd, vl, vr;
			int texturebank;
			int color;
			int cmode;
			int cz_value;
			int cz_type;
			int cz_adjust;
			int objectflags;
			int direct;
			polyvertex v[4];
		} quad;
		struct
		{
			int tile, color;
			int prioverchar;
			int fade_enabled;
			int flipx, flipy;
			int linktype;
			int cols, rows;
			int xpos, ypos;
			int cx_min, cx_max;
			int cy_min, cy_max;
			int sizex, sizey;
			int alpha;
			int cz;
		} sprite;
	} data;
};

#define NODE_CHUNK		4096
#define MAX_NODE_CHUNKS	256
static struct scenenode *node_chunks[MAX_NODE_CHUNKS];
static int node_chunk_count;
static int node_used;		/* nodes taken, over all the chunks */
static struct scenenode scenenode_root;

static struct scenenode *alloc_scenenode(void)
{
	int chunk = node_used / NODE_CHUNK;
	struct scenenode *node;

	if (chunk >= MAX_NODE_CHUNKS)
		return NULL;
	if (chunk >= node_chunk_count)
	{
		node_chunks[chunk] = malloc(NODE_CHUNK * sizeof(struct scenenode));
		if (!node_chunks[chunk])
			return NULL;
		node_chunk_count = chunk + 1;
	}
	node = &node_chunks[chunk][node_used % NODE_CHUNK];
	node_used++;
	memset(node, 0, sizeof(*node));
	return node;
}

static struct scenenode *new_scenenode(UINT32 zsort, int type)
{
	struct scenenode *node = &scenenode_root;
	struct scenenode *prev = NULL;
	struct scenenode *leaf;
	int hash = 0;
	int i;

	for (i = 0; i < 24; i += RADIX_BITS)
	{
		struct scenenode *next;
		hash = (zsort >> (24 - RADIX_BITS)) & RADIX_MASK;
		next = node->data.nonleaf.next[hash];
		if (!next)
		{
			next = alloc_scenenode();
			if (!next)
				return NULL;
			next->type = NODE_NONLEAF;
			node->data.nonleaf.next[hash] = next;
		}
		prev = node;
		node = next;
		zsort <<= RADIX_BITS;
	}

	if (node->type == NODE_NONLEAF)
	{
		/* first leaf on this branch */
		node->type = type;
		return node;
	}

	leaf = alloc_scenenode();
	if (!leaf)
		return NULL;
	leaf->type = type;
	leaf->next = node;
	prev->data.nonleaf.next[hash] = leaf;
	return leaf;
}


/***************************************************************************
    Rasterizer (MAME poly.h, single threaded)
***************************************************************************/

typedef struct
{
	poly3d_t x, y;
	poly3d_t p[4];
} vertex_t;

typedef struct
{
	poly3d_t start, dpdx;
} extent_param;

typedef struct
{
	int startx, stopx;
	extent_param param[4];
} extent_t;

typedef struct
{
	/* poly / sprites */
	int fog_r, fog_g, fog_b;
	const UINT32 *pens;
	int bn;
	int prioverchar;
	int cmode;
	int shade_enabled;
	int texture_enabled;
	int fogfactor;
	/* ss22 */
	int polycolor_r, polycolor_g, polycolor_b;
	int fade_r, fade_g, fade_b;
	int fadefactor;
	int pfade_enabled;
	int alpha_enabled;
	int alpha;
	/* sprites */
	const UINT8 *source;
	int line_modulo;
	int flipx;
	int flipy;
} object_data;

typedef void (*scanline_func)(int scanline, const extent_t *extent, const object_data *extra);

/* where a primitive may draw: its clip rectangle, and the band of the thread
   drawing it (the scanlines y with (y / BAND_STRIP) % nbands == band) */
typedef struct
{
	int minx, maxx, miny, maxy;
	int band, nbands;
} raster_clip;

#define BAND_STRIP_SHIFT	4
#define OWN_LINE(rc, y)	((rc)->nbands == 1 || (((y) >> BAND_STRIP_SHIFT) % (rc)->nbands) == (rc)->band)

/* is row y in this band: strips of 16 scanlines, y / 16 modulo the bands */
#define ROW_OWNED(y, band, nbands)	((nbands) == 1 || (((y) >> BAND_STRIP_SHIFT) % (nbands)) == (band))

static INT32 round_coordinate(poly3d_t value)
{
	poly3d_t ipart, fpart;
	if (value >= 2147483647.0)
		return 2147483647;
	ipart = floor(value);
	if (ipart < -2147483648.0)
		return (-2147483647 - 1);
	fpart = value - ipart;
	return (INT32)ipart + ((fpart > 0.5) ? 1 : 0);
}

static void render_triangle(const raster_clip *rc, scanline_func callback, const object_data *extra, int paramcount,
	const vertex_t *v1, const vertex_t *v2, const vertex_t *v3)
{
	const vertex_t *t;
	INT32 v1y, v3y, v1yclip, v3yclip, curscan;
	poly3d_t dxdy_v1v2, dxdy_v1v3, dxdy_v2v3;
	poly3d_t param_start[4], param_dpdx[4], param_dpdy[4];
	int paramnum;

	if (v2->y < v1->y) { t = v1; v1 = v2; v2 = t; }
	if (v3->y < v2->y)
	{
		t = v2; v2 = v3; v3 = t;
		if (v2->y < v1->y) { t = v1; v1 = v2; v2 = t; }
	}

	v1y = round_coordinate(v1->y);
	v3y = round_coordinate(v3->y);
	v1yclip = (v1y > rc->miny) ? v1y : rc->miny;
	v3yclip = (v3y < rc->maxy + 1) ? v3y : rc->maxy + 1;
	if (v3yclip - v1yclip <= 0)
		return;

	dxdy_v1v2 = (v2->y == v1->y) ? 0.0 : (v2->x - v1->x) / (v2->y - v1->y);
	dxdy_v1v3 = (v3->y == v1->y) ? 0.0 : (v3->x - v1->x) / (v3->y - v1->y);
	dxdy_v2v3 = (v3->y == v2->y) ? 0.0 : (v3->x - v2->x) / (v3->y - v2->y);

	{
		poly3d_t a00 = v2->y - v3->y;
		poly3d_t a01 = v3->x - v2->x;
		poly3d_t a02 = v2->x*v3->y - v3->x*v2->y;
		poly3d_t a10 = v3->y - v1->y;
		poly3d_t a11 = v1->x - v3->x;
		poly3d_t a12 = v3->x*v1->y - v1->x*v3->y;
		poly3d_t a20 = v1->y - v2->y;
		poly3d_t a21 = v2->x - v1->x;
		poly3d_t a22 = v1->x*v2->y - v2->x*v1->y;
		poly3d_t det = a02 + a12 + a22;

		if (fabs(det) < 0.00001)
		{
			for (paramnum = 0; paramnum < paramcount; paramnum++)
			{
				param_dpdx[paramnum] = 0.0;
				param_dpdy[paramnum] = 0.0;
				param_start[paramnum] = v1->p[paramnum];
			}
		}
		else
		{
			poly3d_t idet = 1.0 / det;
			for (paramnum = 0; paramnum < paramcount; paramnum++)
			{
				param_dpdx[paramnum]  = idet * (v1->p[paramnum]*a00 + v2->p[paramnum]*a10 + v3->p[paramnum]*a20);
				param_dpdy[paramnum]  = idet * (v1->p[paramnum]*a01 + v2->p[paramnum]*a11 + v3->p[paramnum]*a21);
				param_start[paramnum] = idet * (v1->p[paramnum]*a02 + v2->p[paramnum]*a12 + v3->p[paramnum]*a22);
			}
		}
	}

	for (curscan = v1yclip; curscan < v3yclip; curscan++)
	{
		extent_t extent;
		poly3d_t fully = (poly3d_t)curscan + 0.5;
		poly3d_t startx = v1->x + (fully - v1->y) * dxdy_v1v3;
		poly3d_t stopx, fullstartx;
		INT32 istartx, istopx;

		if (!OWN_LINE(rc, curscan))
			continue;

		if (fully < v2->y)
			stopx = v1->x + (fully - v1->y) * dxdy_v1v2;
		else
			stopx = v2->x + (fully - v2->y) * dxdy_v2v3;

		istartx = round_coordinate(startx);
		istopx = round_coordinate(stopx);
		if (istartx > istopx) { INT32 tmp = istartx; istartx = istopx; istopx = tmp; }
		if (istartx < rc->minx) istartx = rc->minx;
		if (istopx > rc->maxx + 1) istopx = rc->maxx + 1;
		if (istartx >= istopx)
			continue;

		extent.startx = istartx;
		extent.stopx = istopx;
		fullstartx = (poly3d_t)istartx + 0.5;
		for (paramnum = 0; paramnum < paramcount; paramnum++)
		{
			extent.param[paramnum].start = param_start[paramnum] + fullstartx * param_dpdx[paramnum] + fully * param_dpdy[paramnum];
			extent.param[paramnum].dpdx = param_dpdx[paramnum];
		}
		callback(curscan, &extent, extra);
	}
}

typedef struct
{
	const vertex_t *v1, *v2;
	poly3d_t dxdy;
	poly3d_t dpdy[4];
} poly_edge;

static void render_polygon(const raster_clip *rc, scanline_func callback, const object_data *extra, int paramcount, int numverts, const vertex_t *v)
{
	poly3d_t minx = v[0].x, maxx = v[0].x;
	int minv = 0, maxv = 0;
	int vertnum, curv, paramnum;
	INT32 miny, maxy, minyclip, maxyclip, curscan;
	poly_edge fedgelist[8], bedgelist[8];
	poly_edge *edgeptr;
	const poly_edge *ledge, *redge;

	for (vertnum = 1; vertnum < numverts; vertnum++)
	{
		if (v[vertnum].y < v[minv].y)
			minv = vertnum;
		else if (v[vertnum].y > v[maxv].y)
			maxv = vertnum;
		if (v[vertnum].x < minx) minx = v[vertnum].x;
		if (v[vertnum].x > maxx) maxx = v[vertnum].x;
	}

	miny = round_coordinate(v[minv].y);
	maxy = round_coordinate(v[maxv].y);
	minyclip = (miny > rc->miny) ? miny : rc->miny;
	maxyclip = (maxy < rc->maxy + 1) ? maxy : rc->maxy + 1;
	if (maxyclip - minyclip <= 0)
		return;

	/* forward edges */
	edgeptr = &fedgelist[0];
	for (curv = minv; curv != maxv; curv = (curv == numverts - 1) ? 0 : (curv + 1))
	{
		poly3d_t ooy;
		edgeptr->v1 = &v[curv];
		edgeptr->v2 = &v[(curv == numverts - 1) ? 0 : (curv + 1)];
		if (edgeptr->v1->y == edgeptr->v2->y)
			continue;
		ooy = 1.0 / (edgeptr->v2->y - edgeptr->v1->y);
		edgeptr->dxdy = (edgeptr->v2->x - edgeptr->v1->x) * ooy;
		for (paramnum = 0; paramnum < paramcount; paramnum++)
			edgeptr->dpdy[paramnum] = (edgeptr->v2->p[paramnum] - edgeptr->v1->p[paramnum]) * ooy;
		++edgeptr;
	}

	/* backward edges */
	edgeptr = &bedgelist[0];
	for (curv = minv; curv != maxv; curv = (curv == 0) ? (numverts - 1) : (curv - 1))
	{
		poly3d_t ooy;
		edgeptr->v1 = &v[curv];
		edgeptr->v2 = &v[(curv == 0) ? (numverts - 1) : (curv - 1)];
		if (edgeptr->v1->y == edgeptr->v2->y)
			continue;
		ooy = 1.0 / (edgeptr->v2->y - edgeptr->v1->y);
		edgeptr->dxdy = (edgeptr->v2->x - edgeptr->v1->x) * ooy;
		for (paramnum = 0; paramnum < paramcount; paramnum++)
			edgeptr->dpdy[paramnum] = (edgeptr->v2->p[paramnum] - edgeptr->v1->p[paramnum]) * ooy;
		++edgeptr;
	}

	if ((fedgelist[0].v1 == bedgelist[0].v1 && fedgelist[0].dxdy < bedgelist[0].dxdy) ||
		(fedgelist[0].v1 != bedgelist[0].v1 && fedgelist[0].v1->x < bedgelist[0].v1->x))
	{
		ledge = fedgelist;
		redge = bedgelist;
	}
	else
	{
		ledge = bedgelist;
		redge = fedgelist;
	}

	for (curscan = minyclip; curscan < maxyclip; curscan++)
	{
		extent_t extent;
		poly3d_t fully = (poly3d_t)curscan + 0.5;
		poly3d_t startx, stopx;
		INT32 istartx, istopx;

		while (fully > ledge->v2->y && fully < v[maxv].y)
			++ledge;
		while (fully > redge->v2->y && fully < v[maxv].y)
			++redge;
		if (!OWN_LINE(rc, curscan))
			continue;
		startx = ledge->v1->x + (fully - ledge->v1->y) * ledge->dxdy;
		stopx = redge->v1->x + (fully - redge->v1->y) * redge->dxdy;

		istartx = round_coordinate(startx);
		istopx = round_coordinate(stopx);
		if (istartx > istopx) { INT32 tmp = istartx; istartx = istopx; istopx = tmp; }
		if (istartx < rc->minx) istartx = rc->minx;
		if (istopx > rc->maxx + 1) istopx = rc->maxx + 1;

		if (paramcount > 0)
		{
			poly3d_t ldy = fully - ledge->v1->y;
			poly3d_t rdy = fully - redge->v1->y;
			poly3d_t oox = 1.0 / (stopx - startx);
			for (paramnum = 0; paramnum < paramcount; paramnum++)
			{
				poly3d_t lparam = ledge->v1->p[paramnum] + ldy * ledge->dpdy[paramnum];
				poly3d_t rparam = redge->v1->p[paramnum] + rdy * redge->dpdy[paramnum];
				poly3d_t dpdx = (rparam - lparam) * oox;
				extent.param[paramnum].start = lparam + ((poly3d_t)istartx + 0.5 - startx) * dpdx;
				extent.param[paramnum].dpdx = dpdx;
			}
		}

		if (istartx >= istopx)
			continue;
		extent.startx = istartx;
		extent.stopx = istopx;
		callback(curscan, &extent, extra);
	}
}

static int zclip_if_less(int numverts, const vertex_t *v, vertex_t *outv, int paramcount, poly3d_t clipval)
{
	int prevclipped = (v[numverts - 1].p[0] < clipval);
	vertex_t *nextout = outv;
	int vertnum, paramnum;

	for (vertnum = 0; vertnum < numverts; vertnum++)
	{
		int thisclipped = (v[vertnum].p[0] < clipval);
		if (thisclipped != prevclipped)
		{
			const vertex_t *v1 = &v[(vertnum == 0) ? (numverts - 1) : (vertnum - 1)];
			const vertex_t *v2 = &v[vertnum];
			poly3d_t frac = (clipval - v1->p[0]) / (v2->p[0] - v1->p[0]);
			nextout->x = v1->x + frac * (v2->x - v1->x);
			nextout->y = v1->y + frac * (v2->y - v1->y);
			for (paramnum = 0; paramnum < paramcount; paramnum++)
				nextout->p[paramnum] = v1->p[paramnum] + frac * (v2->p[paramnum] - v1->p[paramnum]);
			++nextout;
		}
		if (!thisclipped)
			*nextout++ = v[vertnum];
		prevclipped = thisclipped;
	}
	return nextout - outv;
}


/***************************************************************************
    Color helpers (rgbaint_t: blend, scale, clamp)
***************************************************************************/

#define RGB_R(c)	(((c) >> 16) & 0xff)
#define RGB_G(c)	(((c) >> 8) & 0xff)
#define RGB_B(c)	((c) & 0xff)

static INLINE int clamp255(int v)
{
	return (v < 0) ? 0 : (v > 255) ? 255 : v;
}

/* blend(other, factor): (c * factor + other * (256 - factor)) >> 8 */
#define BLEND(cr,cg,cb, or_,og,ob, factor) \
	do { int s1_ = (factor), s2_ = 256 - s1_; \
		cr = ((cr * s1_) + ((or_) * s2_)) >> 8; \
		cg = ((cg * s1_) + ((og) * s2_)) >> 8; \
		cb = ((cb * s1_) + ((ob) * s2_)) >> 8; } while (0)

#define SCALE_IMM_CLAMP(cr,cg,cb, s) \
	do { int s_ = (s); \
		cr = clamp255((cr * s_) >> 8); \
		cg = clamp255((cg * s_) >> 8); \
		cb = clamp255((cb * s_) >> 8); } while (0)

#define SCALE_CLAMP(cr,cg,cb, sr,sg,sb) \
	do { cr = clamp255((cr * (sr)) >> 8); \
		cg = clamp255((cg * (sg)) >> 8); \
		cb = clamp255((cb * (sb)) >> 8); } while (0)

#define TO_RGB(cr,cg,cb)	((((cr) & 0xff) << 16) | (((cg) & 0xff) << 8) | ((cb) & 0xff))


/***************************************************************************
    Scanline callbacks
***************************************************************************/

static void setup_cmode(const object_data *extra, const UINT32 **pens_out, int *penmask, int *penshift)
{
	const UINT32 *p = extra->pens;
	*penmask = 0xff;
	*penshift = 0;
	if (extra->cmode & 4)
	{
		p += 0xec + ((extra->cmode & 8) << 1);
		*penmask = 0x03;
		*penshift = 2 * (~extra->cmode & 3);
	}
	else if (extra->cmode & 2)
	{
		p += 0xe0 + ((extra->cmode & 8) << 1);
		*penmask = 0x0f;
		*penshift = 4 * (~extra->cmode & 1);
	}
	*pens_out = p;
}

/* texel lookup: tile32[to] = (tile << 12) | attr */
#define TEXEL(tu_, tv_) \
	{ \
		int tx_ = (int)(tu_) & 0xfff; \
		int ty_ = ((int)(tv_) & 0xfff) | bn; \
		UINT32 e_ = tt[((ty_ << 4) & 0xfff00) | (tx_ >> 4)]; \
		pen = tdata[((e_ >> 4) & 0xffff00) | ayx[((e_ & 0xf) << 8) | ((ty_ << 4) & 0xf0) | (tx_ & 0xf)]]; \
	}

/* Perspective correction: MAME divides by z at every pixel. Here 1/z is exact
   at the ends of spans of SPAN pixels and u/z, v/z, i/z are interpolated
   linearly in between (the texel error stays well under a texel at these
   sizes). Everything the loops use is in locals: the stores to the frame
   could alias the object data otherwise. */
#define SPAN 8
static const float span_inv[SPAN + 1] = { 0.0f, 1.0f, 1.0f/2, 1.0f/3, 1.0f/4, 1.0f/5, 1.0f/6, 1.0f/7, 1.0f/8 };

/* start a span at x: tu/tv/ti and their steps over the next n pixels */
#define SPAN_SETUP() \
	{ \
		const float ooz0 = 1.0f / (float)z; \
		float ooz1; \
		poly3d_t u1, v1, i1; \
		left = stopx - x; \
		if (left > SPAN) left = SPAN; \
		z += left * dz; u1 = u + left * du; v1 = v + left * dv; i1 = i + left * di; \
		ooz1 = 1.0f / (float)z; \
		tu = (float)(u * ooz0); tv = (float)(v * ooz0); ti = (float)(i * ooz0); \
		dtu = ((float)(u1 * ooz1) - tu) * span_inv[left]; \
		dtv = ((float)(v1 * ooz1) - tv) * span_inv[left]; \
		dti = ((float)(i1 * ooz1) - ti) * span_inv[left]; \
		u = u1; v = v1; i = i1; \
	}

static void renderscanline_poly(int scanline, const extent_t *extent, const object_data *extra)
{
	poly3d_t z = extent->param[0].start;
	poly3d_t u = extent->param[1].start;
	poly3d_t v = extent->param[2].start;
	poly3d_t i = extent->param[3].start;
	const poly3d_t dz = extent->param[0].dpdx;
	const poly3d_t du = extent->param[1].dpdx;
	const poly3d_t dv = extent->param[2].dpdx;
	const poly3d_t di = extent->param[3].dpdx;
	const int bn = extra->bn * 0x1000;
	const int fogfactor = 0xff - extra->fogfactor;
	const int fog = (fogfactor != 0xff);
	const int fg_r = extra->fog_r, fg_g = extra->fog_g, fg_b = extra->fog_b;
	const int tex = extra->texture_enabled, shade_en = extra->shade_enabled;
	const int prio = extra->prioverchar;
	const UINT32 *tt = texture_tile32;
	const UINT8 *tdata = texture_tiledata;
	const UINT8 *ayx = texture_ayx_to_pixel;
	int pen = 0, penmask, penshift, x, left = 0;
	float tu = 0, tv = 0, ti = 0, dtu = 0, dtv = 0, dti = 0;
	const int stopx = extent->stopx;
	const UINT32 *ppens;
	UINT32 *dest = frame + scanline * SCREEN_W;
	UINT8 *pri = primap + scanline * SCREEN_W;

	setup_cmode(extra, &ppens, &penmask, &penshift);

	for (x = extent->startx; x < stopx; x++)
	{
		UINT32 c;
		int r, g, b;

		if (left == 0)
			SPAN_SETUP()

		if (tex)
			TEXEL(tu, tv)
		c = ppens[(pen >> penshift) & penmask];
		r = RGB_R(c); g = RGB_G(c); b = RGB_B(c);

		if (fog)
			BLEND(r, g, b, fg_r, fg_g, fg_b, fogfactor);

		/* shading after fog */
		if (shade_en)
		{
			int shade = (int)ti;
			SCALE_IMM_CLAMP(r, g, b, shade << 2);
		}

		dest[x] = TO_RGB(r, g, b);
		pri[x] = (pri[x] & ~1) | prio;

		tu += dtu;
		tv += dtv;
		ti += dti;
		left--;
	}
}

static void renderscanline_poly_ss22(int scanline, const extent_t *extent, const object_data *extra)
{
	poly3d_t z = extent->param[0].start;
	poly3d_t u = extent->param[1].start;
	poly3d_t v = extent->param[2].start;
	poly3d_t i = extent->param[3].start;
	const poly3d_t dz = extent->param[0].dpdx;
	const poly3d_t du = extent->param[1].dpdx;
	const poly3d_t dv = extent->param[2].dpdx;
	const poly3d_t di = extent->param[3].dpdx;
	const int bn = extra->bn * 0x1000;
	const int fogfactor = 0xff - extra->fogfactor;
	const int fadefactor = 0xff - extra->fadefactor;
	const int alphafactor = 0xff - extra->alpha;
	const int fog = (fogfactor != 0xff);
	const int fade = (fadefactor != 0xff);
	const int alpha = (alphafactor != 0xff);
	const int alpha_all = extra->alpha_enabled;
	const int alpha_pen = poly_alpha_pen;
	const int fg_r = extra->fog_r, fg_g = extra->fog_g, fg_b = extra->fog_b;
	const int fade_r = extra->fade_r, fade_g = extra->fade_g, fade_b = extra->fade_b;
	const int pfade = extra->pfade_enabled;
	const int pc_r = extra->polycolor_r, pc_g = extra->polycolor_g, pc_b = extra->polycolor_b;
	const int tex = extra->texture_enabled, shade_en = extra->shade_enabled;
	const int prio = extra->prioverchar;
	const UINT32 *tt = texture_tile32;
	const UINT8 *tdata = texture_tiledata;
	const UINT8 *ayx = texture_ayx_to_pixel;
	int pen = 0, penmask, penshift, x, left = 0;
	float tu = 0, tv = 0, ti = 0, dtu = 0, dtv = 0, dti = 0;
	const int stopx = extent->stopx;
	const UINT32 *ppens;
	UINT32 *dest = frame + scanline * SCREEN_W;
	UINT8 *pri = primap + scanline * SCREEN_W;

	setup_cmode(extra, &ppens, &penmask, &penshift);

	for (x = extent->startx; x < stopx; x++)
	{
		UINT32 c;
		int r, g, b;

		if (left == 0)
			SPAN_SETUP()

		if (tex)
			TEXEL(tu, tv)
		c = ppens[(pen >> penshift) & penmask];
		r = RGB_R(c); g = RGB_G(c); b = RGB_B(c);

		/* shading before fog */
		if (shade_en)
		{
			int shade = (int)ti;
			SCALE_IMM_CLAMP(r, g, b, shade << 2);
		}

		if (fog)
			BLEND(r, g, b, fg_r, fg_g, fg_b, fogfactor);

		if (pfade)
			SCALE_CLAMP(r, g, b, pc_r, pc_g, pc_b);

		if (fade)
			BLEND(r, g, b, fade_r, fade_g, fade_b, fadefactor);

		if (alpha && (alpha_all || pen == alpha_pen))
		{
			UINT32 d = dest[x];
			BLEND(r, g, b, (int)RGB_R(d), (int)RGB_G(d), (int)RGB_B(d), alphafactor);
		}

		dest[x] = TO_RGB(r, g, b);
		pri[x] = (pri[x] & ~1) | prio;

		tu += dtu;
		tv += dtv;
		ti += dti;
		left--;
	}
}

/* the common case: textured and shaded, no poly fade, no global fade, no
   alpha; with or without fog. Super System 22 shades before the fog, System 22
   after it. Same results as the generic loops. */
#define SS22_FAST_BODY(FOG, SHADE_FIRST) \
	poly3d_t z = extent->param[0].start; \
	poly3d_t u = extent->param[1].start; \
	poly3d_t v = extent->param[2].start; \
	poly3d_t i = extent->param[3].start; \
	const poly3d_t dz = extent->param[0].dpdx; \
	const poly3d_t du = extent->param[1].dpdx; \
	const poly3d_t dv = extent->param[2].dpdx; \
	const poly3d_t di = extent->param[3].dpdx; \
	const int bn = extra->bn * 0x1000; \
	const int fogfactor = 0xff - extra->fogfactor; \
	const int fg_r = extra->fog_r, fg_g = extra->fog_g, fg_b = extra->fog_b; \
	const int prio = extra->prioverchar; \
	const UINT32 *tt = texture_tile32; \
	const UINT8 *tdata = texture_tiledata; \
	const UINT8 *ayx = texture_ayx_to_pixel; \
	int pen, penmask, penshift, x, left = 0, lastkey = -1; \
	const UINT8 *tile = tdata, *tileayx = ayx; \
	float tu = 0, tv = 0, ti = 0, dtu = 0, dtv = 0, dti = 0; \
	const int stopx = extent->stopx; \
	const UINT32 *ppens; \
	UINT32 *dest = frame + scanline * SCREEN_W; \
	UINT8 *pri = primap + scanline * SCREEN_W; \
 \
	setup_cmode(extra, &ppens, &penmask, &penshift); \
 \
	for (x = extent->startx; x < stopx; x++) \
	{ \
		UINT32 c; \
		int r, g, b, s; \
 \
		if (left == 0) \
			SPAN_SETUP() \
		{ \
			/* the tile only changes every few pixels */ \
			int tx_ = (int)tu & 0xfff; \
			int ty_ = ((int)tv & 0xfff) | bn; \
			int key_ = ((ty_ << 4) & 0xfff00) | (tx_ >> 4); \
			if (key_ != lastkey) \
			{ \
				UINT32 e_ = tt[key_]; \
				tile = tdata + ((e_ >> 4) & 0xffff00); \
				tileayx = ayx + ((e_ & 0xf) << 8); \
				lastkey = key_; \
			} \
			pen = tile[tileayx[((ty_ << 4) & 0xf0) | (tx_ & 0xf)]]; \
		} \
		c = ppens[(pen >> penshift) & penmask]; \
 \
		r = RGB_R(c); g = RGB_G(c); b = RGB_B(c); \
		if (FOG && !SHADE_FIRST) \
			BLEND(r, g, b, fg_r, fg_g, fg_b, fogfactor); \
 \
		/* shading: a negative factor gives 0, like the clamp */ \
		s = (int)ti << 2; \
		if (s < 0) s = 0; \
		r = (r * s) >> 8; if (r > 255) r = 255; \
		g = (g * s) >> 8; if (g > 255) g = 255; \
		b = (b * s) >> 8; if (b > 255) b = 255; \
 \
		if (FOG && SHADE_FIRST) \
			BLEND(r, g, b, fg_r, fg_g, fg_b, fogfactor); \
 \
		dest[x] = (r << 16) | (g << 8) | b; \
		pri[x] = (pri[x] & ~1) | prio; \
 \
		tu += dtu; \
		tv += dtv; \
		ti += dti; \
		left--; \
	}

static void renderscanline_poly_ss22_fast(int scanline, const extent_t *extent, const object_data *extra)
{
	SS22_FAST_BODY(0, 1)
}

static void renderscanline_poly_ss22_fast_fog(int scanline, const extent_t *extent, const object_data *extra)
{
	SS22_FAST_BODY(1, 1)
}

static void renderscanline_poly_fast(int scanline, const extent_t *extent, const object_data *extra)
{
	SS22_FAST_BODY(0, 0)
}

static void renderscanline_poly_fast_fog(int scanline, const extent_t *extent, const object_data *extra)
{
	SS22_FAST_BODY(1, 0)
}

static void renderscanline_sprite(int scanline, const extent_t *extent, const object_data *extra)
{
	int y_index = (int)(extent->param[1].start - extra->flipy);
	float x_index = (float)(extent->param[0].start - extra->flipx);
	float dx = (float)extent->param[0].dpdx;
	const UINT32 *pal = extra->pens;
	int prioverchar = extra->prioverchar;
	int alphafactor = extra->alpha;
	int alpha_pen = poly_alpha_pen;
	int fogfactor = 0xff - extra->fogfactor;
	int fadefactor = 0xff - extra->fadefactor;
	const UINT8 *source = extra->source + y_index * extra->line_modulo;
	UINT32 *dest = frame + scanline * SCREEN_W;
	UINT8 *pri = primap + scanline * SCREEN_W;
	int x;

	for (x = extent->startx; x < extent->stopx; x++)
	{
		int pen = source[(int)x_index];
		if (pen != 0xff)
		{
			UINT32 c = pal[pen];
			int r = RGB_R(c), g = RGB_G(c), b = RGB_B(c);

			if (fogfactor != 0xff)
				BLEND(r, g, b, extra->fog_r, extra->fog_g, extra->fog_b, fogfactor);

			if (fadefactor != 0xff)
				BLEND(r, g, b, extra->fade_r, extra->fade_g, extra->fade_b, fadefactor);

			if (alphafactor != 0xff && (extra->alpha_enabled || pen == alpha_pen))
				BLEND(r, g, b, (int)RGB_R(dest[x]), (int)RGB_G(dest[x]), (int)RGB_B(dest[x]), alphafactor);

			dest[x] = TO_RGB(r, g, b);
			pri[x] = (pri[x] & ~1) | prioverchar;
		}
		x_index += dx;
	}
}

static void dispatch_scanline_poly(const raster_clip *rc, scanline_func callback, const object_data *extra, int clipverts, const vertex_t *clipv)
{
	switch (clipverts)
	{
		case 3: render_triangle(rc, callback, extra, 4, &clipv[0], &clipv[1], &clipv[2]); break;
		case 4: case 5: case 6: render_polygon(rc, callback, extra, 4, clipverts, clipv); break;
	}
}

/* the clip rectangle of a primitive, on the screen; 0 if empty */
static int make_clip(raster_clip *rc, const raster_clip *band, int minx, int maxx, int miny, int maxy)
{
	rc->minx = (minx > 0) ? minx : 0;
	rc->maxx = (maxx < SCREEN_W - 1) ? maxx : SCREEN_W - 1;
	rc->miny = (miny > 0) ? miny : 0;
	rc->maxy = (maxy < SCREEN_H - 1) ? maxy : SCREEN_H - 1;
	rc->band = band->band;
	rc->nbands = band->nbands;
	return rc->minx <= rc->maxx && rc->miny <= rc->maxy;
}


/***************************************************************************
    Quads and sprites
***************************************************************************/

static void poly3d_drawquad(const raster_clip *band, struct scenenode *node)
{
	vertex_t v[4];
	vertex_t clipv[6];
	int clipverts, vertnum;
	int direct = node->data.quad.direct;
	int cx = 320 + node->data.quad.vx;
	int cy = 240 + node->data.quad.vy;
	int color, cz_value, cz_type, cz_adj, objflags;
	object_data extra;
	raster_clip rc;

	/* scene clip */
	if (!make_clip(&rc, band, cx + node->data.quad.vl, cx - node->data.quad.vr - 1, cy + node->data.quad.vu, cy - node->data.quad.vd - 1))
		return;

	if (!direct)
	{
		/* project and z-clip */
		for (vertnum = 0; vertnum < 4; vertnum++)
		{
			v[vertnum].x = node->data.quad.v[vertnum].x;
			v[vertnum].y = node->data.quad.v[vertnum].y;
			v[vertnum].p[0] = node->data.quad.v[vertnum].z;
			v[vertnum].p[1] = node->data.quad.v[vertnum].u;
			v[vertnum].p[2] = node->data.quad.v[vertnum].v;
			v[vertnum].p[3] = node->data.quad.v[vertnum].bri;
		}

		clipverts = zclip_if_less(4, v, clipv, 4, 0.00001);
		if (clipverts < 3)
			return;

		for (vertnum = 0; vertnum < clipverts; vertnum++)
		{
			poly3d_t ooz = 1.0 / clipv[vertnum].p[0];
			clipv[vertnum].x = cx + clipv[vertnum].x * ooz;
			clipv[vertnum].y = cy - clipv[vertnum].y * ooz;
			clipv[vertnum].p[0] = ooz;
			clipv[vertnum].p[1] = (clipv[vertnum].p[1] + 0.5) * ooz;
			clipv[vertnum].p[2] = (clipv[vertnum].p[2] + 0.5) * ooz;
			clipv[vertnum].p[3] = (clipv[vertnum].p[3] + 0.5) * ooz;
		}
	}
	else
	{
		/* direct: no clipping, z is 1/z */
		clipverts = 4;
		for (vertnum = 0; vertnum < 4; vertnum++)
		{
			poly3d_t ooz = node->data.quad.v[vertnum].z;
			clipv[vertnum].x = cx + node->data.quad.v[vertnum].x;
			clipv[vertnum].y = cy - node->data.quad.v[vertnum].y;
			clipv[vertnum].p[0] = ooz;
			clipv[vertnum].p[1] = (node->data.quad.v[vertnum].u + 0.5) * ooz;
			clipv[vertnum].p[2] = (node->data.quad.v[vertnum].v + 0.5) * ooz;
			clipv[vertnum].p[3] = (node->data.quad.v[vertnum].bri + 0.5) * ooz;
		}
	}

	color = node->data.quad.color;
	cz_value = node->data.quad.cz_value;
	cz_type = node->data.quad.cz_type;
	cz_adj = node->data.quad.cz_adjust;
	objflags = node->data.quad.objectflags;

	memset(&extra, 0, sizeof(extra));
	extra.pfade_enabled = 0;
	extra.alpha_enabled = 0;
	extra.shade_enabled = 1;
	extra.texture_enabled = 1;
	extra.fadefactor = 0;
	extra.fogfactor = 0;

	extra.pens = &pens[(color & 0x7f) << 8];
	extra.bn = node->data.quad.texturebank;
	extra.cmode = node->data.quad.cmode;
	extra.prioverchar = ((node->data.quad.cmode & 7) == 1) ? 1 : 0;
	extra.prioverchar |= namcos22_is_ss22 ? 2 : 0;

	if (namcos22_is_ss22)
	{
		/* global fade */
		if (mixer_flags & 1)
		{
			extra.fadefactor = screen_fade_factor;
			extra.fade_r = screen_fade_r; extra.fade_g = screen_fade_g; extra.fade_b = screen_fade_b;
		}

		/* poly fade */
		extra.pfade_enabled = poly_fade_enabled;
		extra.polycolor_r = poly_fade_r; extra.polycolor_g = poly_fade_g; extra.polycolor_b = poly_fade_b;

		/* alpha */
		extra.alpha = poly_alpha_factor;
		extra.alpha_enabled = (color & 0x7f) != poly_alpha_color;

		/* poly fog */
		if (~color & 0x80)
		{
			int bank = (czattr[6] >> (cz_type * 2)) & 3;
			int bank_enabled = (czattr[4] >> (bank * 4)) & 4;

			if (bank_enabled)
			{
				INT16 delta = czattr[bank];
				int fogfactor;

				/* sign-extend */
				if (delta < 0) delta |= 0xff00;
				else delta &= 0x00ff;

				extra.fog_r = fog_r; extra.fog_g = fog_g; extra.fog_b = fog_b;

				fogfactor = recalc_czram[bank][cz_value] + delta;
				extra.fogfactor = (fogfactor < 0) ? 0 : (fogfactor > 0xff) ? 0xff : fogfactor;
			}
		}
	}
	else
	{
		/* poly fog */
		if (~color & 0x80)
		{
			int cz_color = cz_type & ((fog_colormask >> (8 * (3 - cz_type))) & 0xff);
			extra.fog_r = fog_r_per_cztype[cz_color];
			extra.fog_g = fog_g_per_cztype[cz_color];
			extra.fog_b = fog_b_per_cztype[cz_color];
			extra.fogfactor = NTHBYTE32(namcos22_czram, (cz_type << 13) | cz_value);
		}
	}

	/* disable textures, shading (and maybe more) */
	if (objflags)
	{
		extra.texture_enabled = 0;
		extra.cmode = 0;

		if (objflags & 6)
		{
			/* absolute pen from cz_adjust, no shading */
			extra.pens = &pens[cz_adj & 0x7fff];
			extra.shade_enabled = 0;
		}
		else
			extra.pens += ((cz_adj >> 16) & 0x7f) & (color | 0x1f);
	}

	/* disable poly fog */
	if (cz_adj & 0x800000)
		extra.fogfactor = 0;

	if (namcos22_is_ss22)
	{
		if (extra.texture_enabled && extra.shade_enabled && !extra.pfade_enabled && extra.fadefactor == 0 && extra.alpha == 0)
			dispatch_scanline_poly(&rc, extra.fogfactor ? renderscanline_poly_ss22_fast_fog : renderscanline_poly_ss22_fast, &extra, clipverts, clipv);
		else
			dispatch_scanline_poly(&rc, renderscanline_poly_ss22, &extra, clipverts, clipv);
	}
	else if (extra.texture_enabled && extra.shade_enabled)
		dispatch_scanline_poly(&rc, extra.fogfactor ? renderscanline_poly_fast_fog : renderscanline_poly_fast, &extra, clipverts, clipv);
	else
		dispatch_scanline_poly(&rc, renderscanline_poly, &extra, clipverts, clipv);
}

static void poly3d_drawsprite(const raster_clip *rc, UINT32 code, UINT32 color, int flipx, int flipy, int sx, int sy,
	int scalex, int scaley, int cz_factor, int prioverchar, int fade_enabled, int alpha)
{
	int sprite_screen_height = (scaley * 32 + 0x8000) >> 16;
	int sprite_screen_width = (scalex * 32 + 0x8000) >> 16;

	if (sprite_screen_width && sprite_screen_height)
	{
		poly3d_t fsx = sx, fsy = sy;
		poly3d_t fsw = sprite_screen_width, fsh = sprite_screen_height;
		object_data extra;
		vertex_t vert[4];

		memset(&extra, 0, sizeof(extra));
		extra.fadefactor = 0;
		extra.fogfactor = 0;
		extra.prioverchar = 2 | prioverchar;
		extra.line_modulo = 32;
		extra.flipx = flipx;
		extra.flipy = flipy;
		extra.pens = &pens[(color & 0x7f) << 8];
		extra.source = sprite_data + (code % sprite_count) * 32 * 32;

		vert[0].x = fsx;       vert[0].y = fsy;       vert[0].p[0] = 0;  vert[0].p[1] = 0;
		vert[1].x = fsx + fsw; vert[1].y = fsy;       vert[1].p[0] = 32; vert[1].p[1] = 0;
		vert[2].x = fsx + fsw; vert[2].y = fsy + fsh; vert[2].p[0] = 32; vert[2].p[1] = 32;
		vert[3].x = fsx;       vert[3].y = fsy + fsh; vert[3].p[0] = 0;  vert[3].p[1] = 32;

		/* global fade */
		if ((mixer_flags & 2) || fade_enabled)
		{
			extra.fadefactor = screen_fade_factor;
			extra.fade_r = screen_fade_r; extra.fade_g = screen_fade_g; extra.fade_b = screen_fade_b;
		}

		/* sprite fog */
		if ((~color & 0x80) && cz_factor > 0)
		{
			extra.fogfactor = cz_factor;
			extra.fog_r = fog_r; extra.fog_g = fog_g; extra.fog_b = fog_b;
		}

		/* alpha */
		extra.alpha = alpha;
		extra.alpha_enabled = (color & 0x7f) != poly_alpha_color;

		render_polygon(rc, renderscanline_sprite, &extra, 2, 4, vert);
	}
}

static void render_sprite(const raster_clip *band, struct scenenode *node)
{
	int offset = 0, row, col;
	raster_clip rc;

	if (!make_clip(&rc, band, node->data.sprite.cx_min, node->data.sprite.cx_max, node->data.sprite.cy_min, node->data.sprite.cy_max))
		return;

	for (row = 0; row < node->data.sprite.rows; row++)
	{
		for (col = 0; col < node->data.sprite.cols; col++)
		{
			int code = node->data.sprite.tile;
			if (node->data.sprite.linktype == 0xff)
				code += offset;
			else
				code += NTHWORD32(&namcos22_spriteram[0x800 / 4], offset + node->data.sprite.linktype * 4);

			poly3d_drawsprite(
				&rc,
				code,
				node->data.sprite.color,
				node->data.sprite.flipx,
				node->data.sprite.flipy,
				node->data.sprite.xpos + col * node->data.sprite.sizex,
				node->data.sprite.ypos + row * node->data.sprite.sizey,
				(node->data.sprite.sizex << 16) / 32,
				(node->data.sprite.sizey << 16) / 32,
				node->data.sprite.cz,
				node->data.sprite.prioverchar,
				node->data.sprite.fade_enabled,
				0xff - node->data.sprite.alpha);
			offset++;
		}
	}
}

static void render_scene_nodes(const raster_clip *band, const struct scenenode *node)
{
	if (!node)
		return;

	if (node->type == NODE_NONLEAF)
	{
		int i;
		for (i = RADIX_BUCKETS - 1; i >= 0; i--)
			render_scene_nodes(band, node->data.nonleaf.next[i]);
	}
	else
	{
		while (node)
		{
			if (node->type == NODE_QUAD)
				poly3d_drawquad(band, (struct scenenode *)node);
			else if (node->type == NODE_SPRITE)
				render_sprite(band, (struct scenenode *)node);
			node = node->next;
		}
	}
}

/* the scene, by the scanlines of one band */
static void render_scene_band(int bandnum, int nbands)
{
	raster_clip band;
	int i;

	band.minx = 0;
	band.maxx = SCREEN_W - 1;
	band.miny = 0;
	band.maxy = SCREEN_H - 1;
	band.band = bandnum;
	band.nbands = nbands;
	for (i = RADIX_BUCKETS - 1; i >= 0; i--)
		render_scene_nodes(&band, scenenode_root.data.nonleaf.next[i]);
}

/*
	Threaded video: with the "Threaded Video" core option the scene is drawn
	by several threads, each one on its strips of 16 scanlines (y / 16 modulo
	the threads). The scene and the state it reads stay the same while they
	draw, a pixel is written by one thread only and in the same order: the
	picture is the same as with one thread.
*/
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#define S22_MAX_THREADS	4

static struct
{
	sthread_t *thread[S22_MAX_THREADS];
	int index[S22_MAX_THREADS];
	slock_t *lock;
	scond_t *start, *done;
	int workers;
	int gen;
	int bands;
	int pending;
	int quit;
	void (*job)(int band, int nbands);
} s22_mt;

static void s22_band_worker(void *arg)
{
	const int i = *(const int *)arg;
	int seen = 0;

	slock_lock(s22_mt.lock);
	for (;;)
	{
		while (s22_mt.gen == seen && !s22_mt.quit)
			scond_wait(s22_mt.start, s22_mt.lock);
		if (s22_mt.quit)
			break;
		seen = s22_mt.gen;
		if (i < s22_mt.bands)
		{
			int bands = s22_mt.bands;
			void (*job)(int, int) = s22_mt.job;
			slock_unlock(s22_mt.lock);
			job(i, bands);
			slock_lock(s22_mt.lock);
			if (--s22_mt.pending == 0)
				scond_signal(s22_mt.done);
		}
	}
	slock_unlock(s22_mt.lock);
}

static void s22_threads_stop(void)
{
	int i;
	if (!s22_mt.lock)
		return;
	slock_lock(s22_mt.lock);
	s22_mt.quit = 1;
	scond_broadcast(s22_mt.start);
	slock_unlock(s22_mt.lock);
	for (i = 1; i < s22_mt.workers + 1; i++)
		sthread_join(s22_mt.thread[i]);
	scond_free(s22_mt.start);
	scond_free(s22_mt.done);
	slock_free(s22_mt.lock);
	memset(&s22_mt, 0, sizeof(s22_mt));
}

/* the frame in bands; 0 if there are no threads (the caller draws it alone) */
static int render_frame_threaded(void (*job)(int band, int nbands))
{
	int bands = options.stv_video_threads, i;

	if (bands > S22_MAX_THREADS)
		bands = S22_MAX_THREADS;
	if (bands < 2)
		return 0;
	if (!s22_mt.lock)
	{
		s22_mt.lock = slock_new();
		s22_mt.start = scond_new();
		s22_mt.done = scond_new();
		if (!s22_mt.lock || !s22_mt.start || !s22_mt.done)
			return 0;
	}
	while (s22_mt.workers < bands - 1)
	{
		i = s22_mt.workers + 1;
		s22_mt.index[i] = i;
		s22_mt.thread[i] = sthread_create(s22_band_worker, &s22_mt.index[i]);
		if (!s22_mt.thread[i])
			break;
		s22_mt.workers++;
	}
	if (bands > s22_mt.workers + 1)
		bands = s22_mt.workers + 1;
	if (bands < 2)
		return 0;

	slock_lock(s22_mt.lock);
	s22_mt.job = job;
	s22_mt.bands = bands;
	s22_mt.pending = bands - 1;
	s22_mt.gen++;
	scond_broadcast(s22_mt.start);
	slock_unlock(s22_mt.lock);

	job(0, bands);

	slock_lock(s22_mt.lock);
	while (s22_mt.pending)
		scond_wait(s22_mt.done, s22_mt.lock);
	slock_unlock(s22_mt.lock);
	return 1;
}
#endif

static void render_frame(void (*job)(int band, int nbands))
{
	int i;
#ifdef HAVE_THREADS
	if (!render_frame_threaded(job))
#endif
		job(0, 1);

	for (i = 0; i < RADIX_BUCKETS; i++)
		scenenode_root.data.nonleaf.next[i] = NULL;
	node_used = 0;
}

/* for the core options: is the "Threaded Video" option of any use here */
int namcos22_video_threads_available(void)
{
#ifdef HAVE_THREADS
	return 1;
#else
	return 0;
#endif
}


/***************************************************************************
    Slave DSP simulation: the display list in polygon RAM
***************************************************************************/

static float dspfixed_to_nativefloat(INT16 val)
{
	return val / (float)0x7fff;
}

static float dspfloat_to_nativefloat(UINT32 val)
{
	INT16 mantissa = (INT16)val;
	float result = (float)mantissa;
	int exponent = (val >> 16) & 0x3f;
	while (exponent < 0x2e)
	{
		result /= 2.0f;
		exponent++;
	}
	return result;
}

static void matrix3d_multiply(float a[4][4], float b[4][4])
{
	float result[4][4];
	int row, col, i;
	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
		{
			float sum = 0.0f;
			for (i = 0; i < 4; i++)
				sum += a[row][i] * b[i][col];
			result[row][col] = sum;
		}
	memcpy(a, result, sizeof(result));
}

static void matrix3d_identity(float m[4][4])
{
	int row, col;
	for (row = 0; row < 4; row++)
		for (col = 0; col < 4; col++)
			m[row][col] = (row == col) ? 1.0f : 0.0f;
}

static void matrix3d_apply_reflection(float m[4][4])
{
	float r[4][4];
	if (!reflection)
		return;
	matrix3d_identity(r);
	if (reflection & 0x10) r[0][0] = -1.0f;
	if (reflection & 0x20) r[1][1] = -1.0f;
	matrix3d_multiply(m, r);
}

static void transform_point(float *vx, float *vy, float *vz, float m[4][4])
{
	float x = *vx, y = *vy, z = *vz;
	*vx = m[0][0]*x + m[1][0]*y + m[2][0]*z + m[3][0];
	*vy = m[0][1]*x + m[1][1]*y + m[2][1]*z + m[3][1];
	*vz = m[0][2]*x + m[1][2]*y + m[2][2]*z + m[3][2];
}

static void transform_normal(float *nx, float *ny, float *nz, float m[4][4])
{
	float x = *nx, y = *ny, z = *nz;
	*nx = m[0][0]*x + m[1][0]*y + m[2][0]*z;
	*ny = m[0][1]*x + m[1][1]*y + m[2][1]*z;
	*nz = m[0][2]*x + m[1][2]*y + m[2][2]*z;
}

static void register_normals(int addr, float m[4][4])
{
	int i;
	for (i = 0; i < 4; i++)
	{
		float nx = dspfixed_to_nativefloat(namcos22_point_read(addr + i * 3 + 0));
		float ny = dspfixed_to_nativefloat(namcos22_point_read(addr + i * 3 + 1));
		float nz = dspfixed_to_nativefloat(namcos22_point_read(addr + i * 3 + 2));
		float cxl = camera_lx, cyl = camera_ly, czl = camera_lz;
		float dotproduct;

		transform_normal(&nx, &ny, &nz, m);
		transform_normal(&cxl, &cyl, &czl, viewmatrix);

		dotproduct = nx*cxl + ny*cyl + nz*czl;
		if (dotproduct < 0.0f)
			dotproduct = 0.0f;

		if (lit_surface_count < MAX_LIT_SURFACES)
			lit_surface_info[lit_surface_count++] = (UINT8)(int)(camera_ambient + lit_surface_intensity * dotproduct);
	}
}

void namcos22_draw_direct_poly(const UINT16 *src)
{
	/* the master DSP can write directly to the render device (port 0xc) */
	struct scenenode *node;
	UINT32 zsort;
	int i;

	if (namcos22_is_ss22 && !(NTHBYTE32(namcos22_mixer, 0x1f) & 1))
		return;

	zsort = ((src[1] & 0xfff) << 12) | (src[0] & 0xfff);
	node = new_scenenode(zsort, NODE_QUAD);
	if (!node)
		return;

	if (namcos22_is_ss22)
	{
		node->data.quad.cmode = (src[2] & 0x00f0) >> 4;
		node->data.quad.texturebank = (src[2] & 0x000f);
	}
	else
	{
		node->data.quad.cmode = (src[0 + 4] & 0xf000) >> 12;
		node->data.quad.texturebank = (src[1 + 4] & 0xf000) >> 12;
	}
	node->data.quad.color = (src[2] & 0xff00) >> 8;
	node->data.quad.cz_value = (src[3] >> 2) & 0x1fff;
	node->data.quad.cz_type = src[3] & 3;
	node->data.quad.cz_adjust = 0;
	node->data.quad.objectflags = 0;
	src += 4;

	for (i = 0; i < 4; i++)
	{
		polyvertex *p = &node->data.quad.v[i];
		int mantissa, exponent;

		if (namcos22_is_ss22)
		{
			p->u = src[0] >> 4;
			p->v = src[1] >> 4;
		}
		else
		{
			p->u = src[0] & 0x0fff;
			p->v = src[1] & 0x0fff;
		}

		mantissa = src[5];
		exponent = src[4] & 0x3f;
		if (mantissa)
		{
			p->z = mantissa;
			while (exponent < 0x2e)
			{
				p->z /= 2.00f;
				exponent++;
			}
		}
		else
			p->z = (float)0x10000;

		p->x = (INT16)src[2];
		p->y = -(INT16)src[3];
		p->bri = src[4] >> 8;
		src += 6;
	}

	node->data.quad.direct = 1;
	node->data.quad.vx = 0;
	node->data.quad.vy = 0;
	node->data.quad.vu = -240;
	node->data.quad.vd = -240;
	node->data.quad.vl = -320;
	node->data.quad.vr = -320;
}

static void blit_single_quad(UINT32 color, UINT32 addr, float m[4][4], int polyshift, int flags, int packetformat)
{
	polyvertex v[4];
	float zmin = 0.0f, zmax = 0.0f;
	int zsort = 0, abs_priority, cz_value, i;
	struct scenenode *node;

	for (i = 0; i < 4; i++)
	{
		polyvertex *pv = &v[i];
		pv->x = namcos22_point_read(0x8 + i * 3 + addr);
		pv->y = namcos22_point_read(0x9 + i * 3 + addr);
		pv->z = namcos22_point_read(0xa + i * 3 + addr);
		transform_point(&pv->x, &pv->y, &pv->z, m);
	}

	for (i = 0; i < 4; i++)
	{
		if (i == 0 || v[i].z > zmax) zmax = v[i].z;
		if (i == 0 || v[i].z < zmin) zmin = v[i].z;
	}

	/* fully behind camera */
	if (zmax < 0.0f)
		return;

	/* backface cull one-sided polygons */
	if (flags & 0x20)
	{
		float c1 =
			(v[2].x*((v[0].z*v[1].y)-(v[0].y*v[1].z)))+
			(v[2].y*((v[0].x*v[1].z)-(v[0].z*v[1].x)))+
			(v[2].z*((v[0].y*v[1].x)-(v[0].x*v[1].y)));
		float c2 =
			(v[0].x*((v[2].z*v[3].y)-(v[2].y*v[3].z)))+
			(v[0].y*((v[2].x*v[3].z)-(v[2].z*v[3].x)))+
			(v[0].z*((v[2].y*v[3].x)-(v[2].x*v[3].y)));

		if ((cullflip && c1 <= 0.0f && c2 <= 0.0f) || (!cullflip && c1 >= 0.0f && c2 >= 0.0f))
			return;
	}

	if (zmin < 0.0f) zmin = 0.0f;

	switch (flags & 0x300)
	{
		case 0x000: zsort = (int)(zmin + 0.5f); break;
		case 0x100: zsort = (int)(zmax + 0.5f); break;
		default:    zsort = (int)((zmin + zmax) / 2.0f + 0.5f); break;
	}

	if (zsort > 0x1fffff) zsort = 0x1fffff;
	abs_priority = absolute_priority & 7;

	/* relative: representative z + shift values */
	if (polyshift & 0x200000)
		zsort = polyshift & 0x1fffff;
	else
	{
		zsort += SIGNED18(polyshift);
		abs_priority += (polyshift & 0x1c0000) >> 18;
	}

	if (objectshift & 0x200000)
		zsort = objectshift & 0x1fffff;
	else
	{
		zsort += SIGNED18(objectshift);
		abs_priority += (objectshift & 0x1c0000) >> 18;
	}

	if (zsort < 0) zsort = 0;
	if (zsort > 0x1fffff) zsort = 0x1fffff;
	abs_priority &= 7;
	zsort |= (abs_priority << 21);

	if (zmax < 0.0f) zmax = 0.0f;
	if (zmax > (float)0x1fffff) zmax = (float)0x1fffff;
	cz_value = (int)(zmax + 0.5f);

	/* u, v */
	for (i = 0; i < 4; i++)
	{
		v[i].u = namcos22_point_read(0 + i * 2 + addr);
		v[i].v = namcos22_point_read(1 + i * 2 + addr);
	}

	/* bri */
	if (lit_surface_count > 0)
	{
		if (lit_surface_gouraud)
		{
			int index = lit_surface_triangles ? (lit_surface_index >> 1) : lit_surface_index;
			int normal_index[4];
			normal_index[0] = 0;
			normal_index[1] = lit_surface_width;
			normal_index[2] = lit_surface_width + 1;
			normal_index[3] = 1;
			index = index / (lit_surface_width - 1) + index;

			for (i = 0; i < 4; i++)
				v[i].bri = lit_surface_info[(index + normal_index[i]) & (MAX_LIT_SURFACES - 1)];

			if (lit_surface_triangles)
			{
				if (lit_surface_index & 1)
					v[0].bri = v[1].bri;
				else
					v[2].bri = v[3].bri;
			}
		}
		else
		{
			for (i = 0; i < 4; i++)
				v[i].bri = lit_surface_info[lit_surface_index & (MAX_LIT_SURFACES - 1)];
		}
	}
	else if (packetformat & 0x40)
	{
		for (i = 0; i < 4; i++)
			v[i].bri = (namcos22_point_read(i + addr) >> 16) & 0xff;
	}
	else
	{
		for (i = 0; i < 4; i++)
			v[i].bri = (color >> 16) & 0xff;
	}

	node = new_scenenode(zsort, NODE_QUAD);
	if (!node)
		return;
	node->data.quad.cmode = (v[0].u >> 12) & 0xf;
	node->data.quad.texturebank = (v[0].v >> 12) & 0xf;
	node->data.quad.color = (color >> 8) & 0xff;
	node->data.quad.cz_value = cz_value >> 8;
	node->data.quad.cz_type = (flags >> 10) & 3;
	node->data.quad.cz_adjust = cz_adjust;
	node->data.quad.objectflags = objectflags;

	for (i = 0; i < 4; i++)
	{
		polyvertex *p = &node->data.quad.v[i];
		p->x = v[i].x * camera_zoom;
		p->y = v[i].y * camera_zoom;
		p->z = v[i].z;
		p->u = v[i].u & 0xfff;
		p->v = v[i].v & 0xfff;
		p->bri = v[i].bri;
	}

	node->data.quad.direct = 0;
	node->data.quad.vx = camera_vx;
	node->data.quad.vy = camera_vy;
	node->data.quad.vu = camera_vu;
	node->data.quad.vd = camera_vd;
	node->data.quad.vl = camera_vl;
	node->data.quad.vr = camera_vr;
}

static void blit_quads(int addr, int len, float m[4][4])
{
	int finish = addr + len;

	while (addr < finish)
	{
		int packetlength = namcos22_point_read(addr++);
		int packetformat = namcos22_point_read(addr + 0);
		int flags, color, bias;

		switch (packetlength)
		{
			case 0x17:
				flags = namcos22_point_read(addr + 1);
				color = namcos22_point_read(addr + 2);
				bias = 0;
				blit_single_quad(color, addr + 3, m, bias, flags, packetformat);
				lit_surface_index++;
				break;

			case 0x18:
				flags = namcos22_point_read(addr + 1);
				color = namcos22_point_read(addr + 2);
				bias  = namcos22_point_read(addr + 3);
				blit_single_quad(color, addr + 4, m, bias, flags, packetformat);
				lit_surface_index++;
				break;

			case 0x10:	/* vertex lighting */
				lit_surface_count = 0;
				lit_surface_index = 0;
				lit_surface_gouraud = (namcos22_point_read(addr + 1) & 3) > 0;
				lit_surface_width = ((namcos22_point_read(addr + 1) >> 16) & 0xf) + 1;
				lit_surface_triangles = ((namcos22_point_read(addr + 1) >> 20) & 1) > 0;
				lit_surface_intensity = ((namcos22_point_read(addr + 3) & 0xffff) * camera_power) >> 15;
				register_normals(addr + 4, m);
				break;

			case 0x0d:	/* additional normals */
				register_normals(addr + 1, m);
				break;

			default:
				logerror("blit_quads unknown packet length: addr=0x%06x len=0x%x format=0x%x\n", addr - 1, packetlength, packetformat);
				return;
		}
		addr += packetlength;

		if ((packetformat & 0x800000) && addr != finish)
		{
			logerror("blit_quads unexpected end: addr=0x%06x\n", addr);
			return;
		}
	}
}

static void blit_polyobject(int code, float m[4][4])
{
	int list_addr = namcos22_point_read(code);

	lit_surface_count = 0;
	lit_surface_index = 0;

	for (;;)
	{
		int object_addr = namcos22_point_read(list_addr++);
		UINT32 chunklength;

		if (object_addr < 0)
		{
			if (object_addr == -1)
				break;	/* end of list */
			object_addr &= 0x00ffffff;
		}

		chunklength = namcos22_point_read(object_addr++);
		if (chunklength > 0x100)
		{
			logerror("blit_polyobject bad chunk length: addr=0x%06x len=0x%x\n", object_addr - 1, chunklength);
			break;
		}

		blit_quads(object_addr, chunklength, m);
	}

	/* the flag applies to a single object (timecris stage 1-3 car bonnet) */
	objectflags &= ~2;
}

static void slavesim_handle_bb0003(const INT32 *src)
{
	camera_ambient = (src[0x1] >> 16) & 0xffff;
	camera_power = src[0x1] & 0xffff;

	camera_lx = dspfixed_to_nativefloat(src[0x2]);
	camera_ly = dspfixed_to_nativefloat(src[0x3]);
	camera_lz = dspfixed_to_nativefloat(src[0x4]);

	absolute_priority = (src[0x3] >> 16) & 0xffff;
	camera_vx = SIGNED12(src[0x5] >> 16);
	camera_vy = SIGNED12(src[0x5] & 0xffff);
	camera_zoom = dspfloat_to_nativefloat(src[0x6]);
	camera_vr = (int)(dspfloat_to_nativefloat(src[0x7]) * camera_zoom - 0.5f);
	camera_vl = (int)(dspfloat_to_nativefloat(src[0x8]) * camera_zoom - 0.5f);
	camera_vu = (int)(dspfloat_to_nativefloat(src[0x9]) * camera_zoom - 0.5f);
	camera_vd = (int)(dspfloat_to_nativefloat(src[0xa]) * camera_zoom - 0.5f);

	reflection = (src[0x2] >> 16) & 0x30;
	cullflip = (reflection == 0x10 || reflection == 0x20);

	if (reflection & 0x10)
	{
		int vl = camera_vl;
		camera_vl = camera_vr;
		camera_vr = vl;
	}
	if (reflection & 0x20)
	{
		int vu = camera_vu;
		camera_vu = camera_vd;
		camera_vd = vu;
	}

	viewmatrix[0][0] = dspfixed_to_nativefloat(src[0x0c]);
	viewmatrix[1][0] = dspfixed_to_nativefloat(src[0x0d]);
	viewmatrix[2][0] = dspfixed_to_nativefloat(src[0x0e]);
	viewmatrix[0][1] = dspfixed_to_nativefloat(src[0x0f]);
	viewmatrix[1][1] = dspfixed_to_nativefloat(src[0x10]);
	viewmatrix[2][1] = dspfixed_to_nativefloat(src[0x11]);
	viewmatrix[0][2] = dspfixed_to_nativefloat(src[0x12]);
	viewmatrix[1][2] = dspfixed_to_nativefloat(src[0x13]);
	viewmatrix[2][2] = dspfixed_to_nativefloat(src[0x14]);

	matrix3d_apply_reflection(viewmatrix);

	/* clear model rendering options */
	cz_adjust = 0;
	objectshift = 0;
	objectflags = 0;
}

static void slavesim_handle_200002(const INT32 *src, int code)
{
	if (code == 0x5 || code >= 0x45)
	{
		float m[4][4];	/* row major */

		matrix3d_identity(m);

		m[0][0] = dspfixed_to_nativefloat(src[0x1]);
		m[1][0] = dspfixed_to_nativefloat(src[0x2]);
		m[2][0] = dspfixed_to_nativefloat(src[0x3]);
		m[0][1] = dspfixed_to_nativefloat(src[0x4]);
		m[1][1] = dspfixed_to_nativefloat(src[0x5]);
		m[2][1] = dspfixed_to_nativefloat(src[0x6]);
		m[0][2] = dspfixed_to_nativefloat(src[0x7]);
		m[1][2] = dspfixed_to_nativefloat(src[0x8]);
		m[2][2] = dspfixed_to_nativefloat(src[0x9]);

		m[3][0] = SIGNED24(src[0xa]);	/* xpos */
		m[3][1] = SIGNED24(src[0xb]);	/* ypos */
		m[3][2] = SIGNED24(src[0xc]);	/* zpos */

		matrix3d_multiply(m, viewmatrix);
		blit_polyobject(code, m);
	}
	else if (code != 0 && code != 2)
		logerror("slavesim_handle_200002:unknown code=0x%x\n", code);
}

static void slavesim_handle_300000(const INT32 *src)
{
	viewmatrix[0][0] = dspfixed_to_nativefloat(src[1]);
	viewmatrix[1][0] = dspfixed_to_nativefloat(src[2]);
	viewmatrix[2][0] = dspfixed_to_nativefloat(src[3]);
	viewmatrix[0][1] = dspfixed_to_nativefloat(src[4]);
	viewmatrix[1][1] = dspfixed_to_nativefloat(src[5]);
	viewmatrix[2][1] = dspfixed_to_nativefloat(src[6]);
	viewmatrix[0][2] = dspfixed_to_nativefloat(src[7]);
	viewmatrix[1][2] = dspfixed_to_nativefloat(src[8]);
	viewmatrix[2][2] = dspfixed_to_nativefloat(src[9]);

	matrix3d_apply_reflection(viewmatrix);
}

static void slavesim_handle_233002(const INT32 *src)
{
	cz_adjust = src[1] & 0xffffff;
	objectshift = src[2] & 0xffffff;
	objectflags = (src[3] >> 21) & 7;
}

static void simulate_slavedsp(void)
{
	const INT32 *base = (const INT32 *)namcos22_polygonram;
	const INT32 *src = base + 0x300;

	if (namcos22_is_ss22)
		src += 4;	/* FFFE 0400 */
	else
		src--;

	for (;;)
	{
		UINT16 code, len, next;
		INT32 index;

		code = *src++;
		len = *src++;
		index = src - base;

		/* alpinr2b titlescreen includes commands to modify pointram on the fly */
		if (namcos22_gametype == NAMCOS22_ALPINE_RACER_2 && code == 0xfff8)
		{
			namcos22_pdp_handle_commands(index - 2);
			src += 0x56;
			continue;
		}
		else if ((index + len) >= 0x7fff)
		{
			logerror("simulate_slavedsp buffer overflow: len=0x%x code=0x%x addr=0x%x\n", len, code, index);
			return;
		}

		switch (len)
		{
			case 0x15: slavesim_handle_bb0003(src); break;		/* define viewport */
			case 0x10: slavesim_handle_233002(src); break;		/* model rendering options */
			case 0x0a: slavesim_handle_300000(src); break;		/* modify view transform */
			case 0x0d: slavesim_handle_200002(src, code); break;	/* render primitive */
			default:
				logerror("simulate_slavedsp unknown 3d data: len=0x%x code=0x%x addr=0x%x\n", len, code, index);
				return;
		}

		src += len;
		src++;	/* should be 0xffff (GOTO command) */
		next = *src++ & 0x7fff;	/* link to next command */
		if (next != (index + len + 1 + 1))
		{
			/* end of list, normally with a "goto self" */
			if (next != (index + len))
				logerror("simulate_slavedsp unexpected end: addr=0x%x, next=0x%x\n", index + len, next);
			return;
		}
		else if (next == 0x7fff)
		{
			logerror("simulate_slavedsp buffer overflow: next=0x7fff\n");
			return;
		}
	}
}

static void draw_polygons(void)
{
	if (namcos22_pdp_render_done && namcos22_slave_simulation_active)
		simulate_slavedsp();
}

static void render_frame_active(void)
{
	/* the PDP and simulate_slavedsp are the same thing: when the PDP is inactive the
	   parser is too (propcycl attract mode expects the previous scene) */
	if (cpu_getcurrentframe() > namcos22_pdp_frame && namcos22_render_refresh)
		namcos22_pdp_render_done = 0;

	namcos22_render_refresh = 0;
}


/***************************************************************************
    Sprites (Super System 22)
***************************************************************************/

static void draw_sprite_group(const UINT32 *src, const UINT32 *attr, int num_sprites, int deltax, int deltay, int y_lowres)
{
	int i;
	for (i = 0; i < num_sprites; i++)
	{
		int xpos = (src[0] >> 16) - deltax;
		int ypos = (src[0] & 0xffff) - deltay;
		int sizex = src[1] >> 16;
		int sizey = src[1] & 0xffff;
		int flipy = (src[2] >> 3) & 1;
		int rows = src[2] & 0x7;
		int linktype = (src[2] >> 16) & 0xff;
		int flipx = (src[2] >> 7) & 1;
		int cols = (src[2] >> 4) & 0x7;
		UINT32 code = src[3];
		int tile = code >> 16;
		int alpha = (code >> 8) & 0xff;

		UINT32 zcoord = attr[0] & 0x00ffffff;
		int color = (attr[1] >> 16) & 0xff;
		int cz = attr[1] & 0xff;
		int fade_enabled = (attr[1] >> 15) & 1;
		int prioverchar = (cz == 0xfe) ? 1 : 0;

		/* window clipping */
		int clip = (src[2] >> 23) & 0xe;
		int cx_min = -deltax + (INT16)(namcos22_spriteram[0x80 | clip] >> 16);
		int cx_max = -deltax + (INT16)(namcos22_spriteram[0x80 | clip] & 0xffff);
		int cy_min = -deltay + (INT16)(namcos22_spriteram[0x81 | clip] >> 16);
		int cy_max = -deltay + (INT16)(namcos22_spriteram[0x81 | clip] & 0xffff);

		if (rows == 0) rows = 8;
		if (cols == 0) cols = 8;

		if (src[2] & 0x200)	/* right justify */
			xpos -= sizex * cols - 1;
		if (src[2] & 0x100)	/* bottom justify */
			ypos -= sizey * rows - 1;

		if (flipy)
		{
			ypos += sizey * rows - 1;
			sizey = -sizey;
		}
		if (flipx)
		{
			xpos += sizex * cols - 1;
			sizex = -sizex;
		}

		if (y_lowres)
		{
			sizey *= 2;
			ypos *= 2;
		}

		if (sizex && sizey)
		{
			struct scenenode *node = new_scenenode(zcoord, NODE_SPRITE);
			if (node)
			{
				node->data.sprite.tile = tile;
				node->data.sprite.flipx = flipx;
				node->data.sprite.flipy = flipy;
				node->data.sprite.cols = cols;
				node->data.sprite.rows = rows;
				node->data.sprite.linktype = linktype;
				node->data.sprite.xpos = xpos;
				node->data.sprite.ypos = ypos;
				node->data.sprite.cx_min = cx_min;
				node->data.sprite.cx_max = cx_max;
				node->data.sprite.cy_min = cy_min;
				node->data.sprite.cy_max = cy_max;
				node->data.sprite.sizex = sizex;
				node->data.sprite.sizey = sizey;
				node->data.sprite.alpha = alpha;
				node->data.sprite.color = color;
				node->data.sprite.cz = cz;
				node->data.sprite.prioverchar = prioverchar;
				node->data.sprite.fade_enabled = fade_enabled;
			}
		}

		src += 4;
		attr += 2;
	}
}

static void draw_sprites(void)
{
	const UINT32 *spr = namcos22_spriteram;
	const UINT32 *vc = namcos22_vics_control;
	int sprites_on = (~spr[0] >> 16) & 1;
	int y_lowres = (~spr[0] >> 18) & 1;
	int deltax = (spr[1] & 0xffff) + (spr[2] & 0xffff) + 0x2d;
	int deltay = (spr[3] >> 16) + (0x2a >> (y_lowres ? 1 : 0));
	int base = spr[0] & 0xffff;
	int num_sprites = ((spr[1] >> 16) - base) + 1;

	if (namcos22_gametype == NAMCOS22_AIR_COMBAT22)
		num_sprites = 0;

	if (sprites_on && num_sprites > 0 && num_sprites < 0x400)
		draw_sprite_group(&spr[0x04000 / 4 + base * 4], &spr[0x20000 / 4 + base * 2], num_sprites, deltax, deltay, y_lowres);

	/* VICS RAM: two more banks */
	sprites_on = (~vc[0x30 / 4] >> 24) & 1;
	y_lowres = (~vc[0x30 / 4] >> 26) & 1;

	num_sprites = (vc[0x40 / 4] >> 4) & 0x1ff;
	if (namcos22_gametype == NAMCOS22_DIRT_DASH)
		num_sprites = (namcos22_vics_data[(vc[0x48 / 4] & 0x4000) / 4] & 0xff) + 1;

	if (sprites_on && num_sprites > 0)
		draw_sprite_group(&namcos22_vics_data[(vc[0x48 / 4] & 0xffff) / 4], &namcos22_vics_data[(vc[0x58 / 4] & 0xffff) / 4], num_sprites, deltax, deltay, y_lowres);

	num_sprites = (vc[0x60 / 4] >> 4) & 0x1ff;
	if (namcos22_gametype == NAMCOS22_AIR_COMBAT22)
	{
		sprites_on = (~namcos22_vics_data[(vc[0x48 / 4] & 0xffff) / 4] >> 16) & 1;
		num_sprites = (namcos22_vics_data[(vc[0x48 / 4] & 0xffff) / 4 + 1] >> 16) + 1;
	}

	if (sprites_on && num_sprites > 0)
		draw_sprite_group(&namcos22_vics_data[(vc[0x68 / 4] & 0xffff) / 4], &namcos22_vics_data[(vc[0x78 / 4] & 0xffff) / 4], num_sprites, deltax, deltay, y_lowres);
}

READ32_HANDLER( namcos22s_vics_control_r )
{
	data32_t ret = namcos22_vics_control[offset];
	switch (offset * 4)
	{
		case 0x00:	/* status register? */
			ret = 0;
			break;
		case 0x40: case 0x50: case 0x60: case 0x70:	/* high bit is busy/ready? */
			ret &= 0x7fffffff;
			break;
	}
	return ret;
}

WRITE32_HANDLER( namcos22s_vics_control_w )
{
	COMBINE_DATA(&namcos22_vics_control[offset]);
}


/***************************************************************************
    Text layer
***************************************************************************/

WRITE32_HANDLER( namcos22_cgram_w )
{
	COMBINE_DATA(&namcos22_cgram[offset]);
}

static void posirq_update(void)
{
	int scanline = 2 * tilemapattr[4];

	if (scanline < 525)
	{
		scanline = (scanline + 480) % 525;	/* 0 = vblank start */
		timer_adjust(posirq_timer, cpu_getscanlinetime(scanline), 0, 0);
	}
	else
		timer_adjust(posirq_timer, TIME_NEVER, 0, 0);
}

static void posirq_callback(int param)
{
	/* hblank irq line differs between s22/ss22 (see syscon) */
	int line = namcos22_is_ss22 ? 1 : 0;

	if (namcos22_irq_enabled & (1 << line))
	{
		namcos22_irq_state |= (1 << line);
		if (namcos22_syscontrol[line] & 7)
			cpu_set_irq_line(0, namcos22_syscontrol[line] & 7, ASSERT_LINE);
	}
	posirq_update();
}

static void update_text_rowscroll(void)
{
	UINT32 fr = cpu_getcurrentframe();
	int scroll_x, y, i;

	if (fr != rs_frame)
	{
		rs_frame = fr;
		lastrow = 0;
	}

	scroll_x = (tilemapattr[0] - 0x35c) & 0x3ff;
	y = cpu_getscanline();
	if (y > 480) y = 480;

	for (i = lastrow; i < y; i++)
		rowscroll[i] = scroll_x;
	lastrow = y;
}

static void tilemapattr_word_w(int offset, UINT16 data, UINT16 keep)
{
	/* 0: x offset, 1: y offset, 4: posirq scanline */
	if (offset == 0)
		update_text_rowscroll();	/* alpines changes x scroll mid-screen */

	tilemapattr[offset] = (tilemapattr[offset] & keep) | (data & ~keep);

	if (offset == 4)
		posirq_update();
}

static UINT16 tilemapattr_word_r(int offset)
{
	switch (offset)
	{
		case 5: return 0x8000;	/* dirtdash slows down if clear */
		case 7: return 0;		/* timecris reads it when the gun triggers */
	}
	return tilemapattr[offset];
}

READ32_HANDLER( namcos22_tilemapattr_r )
{
	return (tilemapattr_word_r(offset * 2) << 16) | tilemapattr_word_r(offset * 2 + 1);
}

WRITE32_HANDLER( namcos22_tilemapattr_w )
{
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		tilemapattr_word_w(offset * 2, data >> 16, mem_mask >> 16);
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		tilemapattr_word_w(offset * 2 + 1, data & 0xffff, mem_mask & 0xffff);
}

/* the text layer scroll of this frame (main thread, before the bands) */
static int text_scroll_y;
static int text_rowx[1024];

static void prepare_text_tilemap(void)
{
	int i;

	update_text_rowscroll();
	text_scroll_y = tilemapattr[1] & 0x3ff;
	for (i = 0; i < 1024; i++)
		text_rowx[i] = rowscroll[0];
	for (i = 0; i < 480; i++)
		text_rowx[(i + text_scroll_y + 4) & 0x3ff] = rowscroll[i];
}

/* render the 64x64 tilemap of 16x16x4 RAM tiles into mix_bitmap (pens) and primap, rows of a band */
static void draw_text_tilemap(int band, int nbands, int prio, int primask)
{
	const int scroll_y = text_scroll_y;
	const UINT32 *textram = &namcos22_cgram[0x1e000 / 4];
	int y;

	for (y = 0; y < SCREEN_H; y++)
	{
		int ty = (y + scroll_y) & 0x3ff;
		int sx = text_rowx[ty];
		UINT16 *dst = mix_bitmap + y * SCREEN_W;
		UINT8 *pri = primap + y * SCREEN_W;
		int x = 0;

		if (!ROW_OWNED(y, band, nbands))
			continue;
		text_row_used[y] = 0;

		while (x < SCREEN_W)
		{
			int tx = (x + sx) & 0x3ff;
			int tile_index = (ty >> 4) * 64 + (tx >> 4);
			UINT16 data = NTHWORD32(textram, tile_index);
			int code = data & 0x3ff;
			int color = data >> 12;
			int flipx = (data >> 10) & 1;
			int flipy = (data >> 11) & 1;
			int row = flipy ? 15 - (ty & 15) : (ty & 15);
			const UINT32 *rowdata = &namcos22_cgram[code * 32 + row * 2];
			int pal = text_palbase + color * 16;
			int px = tx & 15;
			int n = 16 - px;

			if (n > SCREEN_W - x)
				n = SCREEN_W - x;
			if ((rowdata[0] & rowdata[1]) == 0xffffffff)
			{
				x += n;		/* all pens 0xf: transparent */
				continue;
			}
			text_row_used[y] = 1;
			for (; n > 0; n--, px++, x++)
			{
				int cx = flipx ? 15 - px : px;
				int pix = (rowdata[cx >> 3] >> (28 - 4 * (cx & 7))) & 0xf;
				if (pix != 0xf)
				{
					dst[x] = pal + pix;
					pri[x] = (pri[x] & primask) | prio;
				}
			}
		}
	}
}


/***************************************************************************
    Spot RAM, CZ RAM, palette, mixer
***************************************************************************/

READ32_HANDLER( namcos22s_spotram_r )
{
	/* 0x860004: read data */
	if (offset == 1 && !(mem_mask & 0xffff0000))
	{
		UINT16 ret = spotram[(spotram_address >> 1) & 0x7ff];
		spotram_address += 2;
		return ret << 16;
	}
	return 0;
}

WRITE32_HANDLER( namcos22s_spotram_w )
{
	int word;
	for (word = 0; word < 2; word++)
	{
		UINT16 keep = (word == 0) ? (mem_mask >> 16) : (mem_mask & 0xffff);
		UINT16 d = (word == 0) ? (data >> 16) : (data & 0xffff);
		if (keep == 0xffff)
			continue;
		switch (offset * 2 + word)
		{
			case 0:	/* set address */
				spotram_address = (spotram_address & keep) | (d & ~keep);
				break;
			case 1:	/* write */
				spotram[(spotram_address >> 1) & 0x7ff] = (spotram[(spotram_address >> 1) & 0x7ff] & keep) | (d & ~keep);
				spotram_address += 2;
				break;
			case 3:	/* enable */
				spotram_enable = (spotram_enable & keep) | (d & ~keep);
				break;
		}
	}
}

WRITE32_HANDLER( namcos22s_chipselect_w )
{
	if (!(mem_mask & 0x00ff0000))
		chipselect = data >> 16;
	else if (!(mem_mask & 0xff000000))
		chipselect = data >> 24;
}

WRITE32_HANDLER( namcos22_paletteram_w )
{
	data32_t prev = namcos22_paletteram[offset];
	COMBINE_DATA(&namcos22_paletteram[offset]);
	if (prev != namcos22_paletteram[offset])
		dirtypal[offset & (0x7fff / 4)] = 1;
}

static void update_palette(void)
{
	int i, j;
	for (i = 0; i < 0x8000 / 4; i++)
	{
		if (dirtypal[i])
		{
			for (j = 0; j < 4; j++)
			{
				int which = i * 4 + j;
				int r = NTHBYTE32(namcos22_paletteram, which + 0x00000);
				int g = NTHBYTE32(namcos22_paletteram, which + 0x08000);
				int b = NTHBYTE32(namcos22_paletteram, which + 0x10000);
				pens[which] = (r << 16) | (g << 8) | b;
			}
			dirtypal[i] = 0;
		}
	}
}

static void czattr_word_w(int offset, UINT16 data, UINT16 keep)
{
	UINT16 prev = czattr[offset];
	czattr[offset] = (czattr[offset] & keep) | (data & ~keep);

	if (offset == 4)
	{
		/* invalidate if the compare function changed */
		UINT16 changed = prev ^ czattr[offset];
		int bank;
		for (bank = 0; bank < 4; bank++)
			cz_was_written[bank] |= (changed >> (bank * 4)) & 2;
	}
}

READ32_HANDLER( namcos22s_czattr_r )
{
	return (czattr[offset * 2] << 16) | czattr[offset * 2 + 1];
}

WRITE32_HANDLER( namcos22s_czattr_w )
{
	if ((mem_mask & 0xffff0000) != 0xffff0000)
		czattr_word_w(offset * 2, data >> 16, mem_mask >> 16);
	if ((mem_mask & 0x0000ffff) != 0x0000ffff)
		czattr_word_w(offset * 2 + 1, data & 0xffff, mem_mask & 0xffff);
}

WRITE32_HANDLER( namcos22s_czram_w )
{
	int bank;
	for (bank = 0; bank < 4; bank++)
	{
		/* write enable bit */
		if (~czattr[4] >> (bank * 4) & 1)
		{
			UINT32 prev = (banked_czram[bank][offset * 2] << 16) | banked_czram[bank][offset * 2 + 1];
			UINT32 temp = prev;
			COMBINE_DATA(&temp);
			banked_czram[bank][offset * 2] = temp >> 16;
			banked_czram[bank][offset * 2 + 1] = temp & 0xffff;
			cz_was_written[bank] |= (prev ^ temp);
		}
	}
}

READ32_HANDLER( namcos22s_czram_r )
{
	int bank = czattr[5] & 3;
	return (banked_czram[bank][offset * 2] << 16) | banked_czram[bank][offset * 2 + 1];
}

static void recalc_czram_tables(void)
{
	int bank;
	for (bank = 0; bank < 4; bank++)
	{
		/* the SS22 czram is a big compare table: make it a lookup table */
		if (cz_was_written[bank])
		{
			int reverse = (czattr[4] >> (bank * 4) & 2) ? 0xff : 0;
			int small_val = 0x2000;
			int small_offset = reverse;
			int large_val = 0;
			int large_offset = reverse ^ 0xff;
			int prev = 0;
			int i, j;

			for (i = 0; i < 0x100; i++)
			{
				int factor = i ^ reverse;
				int val = banked_czram[bank][factor];
				int start = prev;
				int end;
				if (val > 0x2000) val = 0x2000;
				end = val;

				if (i > 0)
				{
					/* discard if the compare function doesn't match */
					if (start >= end)
						continue;
					for (j = start; j < end; j++)
						recalc_czram[bank][j] = factor;
				}

				if (val < small_val)
				{
					small_val = val;
					small_offset = factor;
				}
				if (val > large_val)
				{
					large_val = val;
					large_offset = factor;
				}
				prev = val;
			}

			for (j = 0; j < small_val; j++)
				recalc_czram[bank][j] = small_offset;
			for (j = large_val; j < 0x2000; j++)
				recalc_czram[bank][j] = large_offset;

			cz_was_written[bank] = 0;
		}
	}
}

static void update_mixer(void)
{
	const UINT32 *m = namcos22_mixer;

	if (namcos22_is_ss22)
	{
		poly_fade_r        = NTHBYTE32(m, 0x00);
		poly_fade_g        = NTHBYTE32(m, 0x01);
		poly_fade_b        = NTHBYTE32(m, 0x02);
		fog_r              = NTHBYTE32(m, 0x05);
		fog_g              = NTHBYTE32(m, 0x06);
		fog_b              = NTHBYTE32(m, 0x07);
		spot_factor        = (NTHBYTE32(m, 0x0e) << 8) | NTHBYTE32(m, 0x0d);
		poly_alpha_color   = NTHBYTE32(m, 0x0f);
		poly_alpha_pen     = NTHBYTE32(m, 0x10);
		poly_alpha_factor  = NTHBYTE32(m, 0x11);
		screen_fade_r      = NTHBYTE32(m, 0x16);
		screen_fade_g      = NTHBYTE32(m, 0x17);
		screen_fade_b      = NTHBYTE32(m, 0x18);
		screen_fade_factor = NTHBYTE32(m, 0x19);
		mixer_flags        = NTHBYTE32(m, 0x1a);
		text_palbase       = (NTHBYTE32(m, 0x1b) << 8) & 0x7f00;

		poly_fade_enabled = (m[0] & 0xffffff00) != 0xffffff00;
	}
	else
	{
		int i;
		mixer_flags   = (NTHBYTE32(m, 0x00) << 8) | NTHBYTE32(m, 0x01);
		bg_palbase    = (NTHBYTE32(m, 0x04) << 8) & 0x7f00;
		text_palbase  = (NTHBYTE32(m, 0x07) << 8) & 0x7f00;
		screen_fade_r = (NTHBYTE32(m, 0x11) << 8) | NTHBYTE32(m, 0x12);
		screen_fade_g = (NTHBYTE32(m, 0x13) << 8) | NTHBYTE32(m, 0x14);
		screen_fade_b = (NTHBYTE32(m, 0x15) << 8) | NTHBYTE32(m, 0x16);
		fog_colormask = m[0x84 / 4];
		for (i = 0; i < 4; i++)
		{
			fog_r_per_cztype[i] = NTHBYTE32(m, 0x0100 + i);
			fog_g_per_cztype[i] = NTHBYTE32(m, 0x0180 + i);
			fog_b_per_cztype[i] = NTHBYTE32(m, 0x0200 + i);
		}
	}
}

static void namcos22s_mix_text_layer(int band, int nbands, int prival)
{
	int alpha_check12 = NTHBYTE32(namcos22_mixer, 0x12);
	int alpha_check13 = NTHBYTE32(namcos22_mixer, 0x13);
	int alpha_mask    = NTHBYTE32(namcos22_mixer, 0x14) & 0xf;
	int alpha_factor  = NTHBYTE32(namcos22_mixer, 0x15);
	int spot_enabled = (spotram_enable & 1) && (chipselect & 0xc000);
	int spot_fac = (spot_factor < 0x100) ? 0 : spot_factor & 0xff;
	int spot_palbase = (text_palbase >> 8) & 3;
	int fade_enabled = (mixer_flags & 2) && screen_fade_factor;
	int fade_factor = 0xff - screen_fade_factor;
	int x, y;

	for (y = 0; y < SCREEN_H; y++)
	{
		const UINT16 *src = mix_bitmap + y * SCREEN_W;
		UINT32 *dest = frame + y * SCREEN_W;
		const UINT8 *pri = primap + y * SCREEN_W;
		if (!text_row_used[y] || !ROW_OWNED(y, band, nbands))
			continue;
		for (x = 0; x < SCREEN_W; x++)
		{
			int pen = 0, r, g, b;
			UINT32 c;

			if (pri[x] != prival)
				continue;

			if (spot_enabled)
			{
				pen = spotram[((src[x] << 2) | spot_palbase) & 0x3ff] & 0xff;
				if (pen < 0x80)
					c = pens[pen | text_palbase];
				else if (prival != 6)
					continue;
				else
					c = 0;
			}
			else
			{
				c = pens[src[x]];
				pen = src[x] & 0xff;
			}
			r = RGB_R(c); g = RGB_G(c); b = RGB_B(c);

			if (spot_enabled && pen >= 0x80)
			{
				/* spot pen becomes brightness factor */
				int factor = (spot_fac * (pen & 0x7f)) >> 7;
				r = RGB_R(dest[x]); g = RGB_G(dest[x]); b = RGB_B(dest[x]);
				SCALE_IMM_CLAMP(r, g, b, 0x100 - factor);
			}
			else
			{
				if (fade_enabled)
					BLEND(r, g, b, screen_fade_r, screen_fade_g, screen_fade_b, fade_factor);

				if (alpha_factor && ((pen & 0xf) == alpha_mask || (pen >= alpha_check12 && pen <= alpha_check13)))
					BLEND(r, g, b, (int)RGB_R(dest[x]), (int)RGB_G(dest[x]), (int)RGB_B(dest[x]), 0xff - alpha_factor);
			}

			dest[x] = TO_RGB(r, g, b);
		}
	}
}


/* System 22: text layer over the scene, shadow pens, global fade and the gamma PROMs */
static void namcos22_mix_text_layer(int band, int nbands, struct mame_bitmap *bitmap, const struct rectangle *cliprect)
{
	int fade_enabled = (screen_fade_r != 0x100 || screen_fade_g != 0x100 || screen_fade_b != 0x100);
	UINT32 fade_r_add = (screen_fade_r > 0x100) ? (1 << 16) : 0;
	UINT32 fade_g_add = (screen_fade_g > 0x100) ? (1 << 8) : 0;
	UINT32 fade_b_add = (screen_fade_b > 0x100) ? 1 : 0;
	int fade_white = fade_r_add || fade_g_add || fade_b_add;
	int shadow_enabled = (mixer_flags >> 8) & 1;	/* ridgerac is the only game not using shadow */
	int mix_r[3], mix_g[3], mix_b[3];
	UINT32 *rl = gamma_lut[0], *gl = gamma_lut[1], *bl = gamma_lut[2];
	int i, x, y;

	for (i = 0; i < 3; i++)
	{
		mix_r[i] = NTHBYTE32(namcos22_mixer, 0x08 + i * 3);
		mix_g[i] = NTHBYTE32(namcos22_mixer, 0x09 + i * 3);
		mix_b[i] = NTHBYTE32(namcos22_mixer, 0x0a + i * 3);
	}

	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const UINT16 *src = mix_bitmap + y * SCREEN_W;
		const UINT8 *pri = primap + y * SCREEN_W;
		const UINT32 *fr = frame + y * SCREEN_W;
		UINT32 *dest = (UINT32 *)bitmap->line[y];

		if (!ROW_OWNED(y, band, nbands))
			continue;

		if (!fade_enabled && !text_row_used[y])
		{
			/* no text on this row: gamma only */
			const UINT32 *rp = rl, *gp = gl, *bp = bl;
			const UINT32 *s_ = fr + cliprect->min_x;
			UINT32 *d_ = dest + cliprect->min_x;
			int n;
			for (n = cliprect->max_x - cliprect->min_x + 1; n > 0; n--)
			{
				UINT32 rgb = *s_++;
				*d_++ = rp[(rgb >> 16) & 0xff] | gp[(rgb >> 8) & 0xff] | bp[rgb & 0xff];
			}
			continue;
		}

		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			UINT32 pixel = fr[x];

			/* skip if transparent or under poly */
			if (pri[x] == 2)
			{
				int pen = src[x] & 0xff;
				if (shadow_enabled && pen >= 0xfc && pen <= 0xfe)
				{
					int r = RGB_R(pixel), g = RGB_G(pixel), b = RGB_B(pixel);
					SCALE_CLAMP(r, g, b, mix_r[pen - 0xfc], mix_g[pen - 0xfc], mix_b[pen - 0xfc]);
					pixel = TO_RGB(r, g, b);
				}
				else
					pixel = pens[src[x]];
			}

			/* global fade */
			if (fade_enabled)
			{
				int r, g, b;
				if (fade_white)
				{
					/* blacks must be able to scale up */
					if (!(pixel & 0xff0000)) pixel += fade_r_add;
					if (!(pixel & 0x00ff00)) pixel += fade_g_add;
					if (!(pixel & 0x0000ff)) pixel += fade_b_add;
				}
				r = RGB_R(pixel); g = RGB_G(pixel); b = RGB_B(pixel);
				SCALE_CLAMP(r, g, b, screen_fade_r, screen_fade_g, screen_fade_b);
				pixel = TO_RGB(r, g, b);
			}

			/* gamma */
			dest[x] = rl[(pixel >> 16) & 0xff] | gl[(pixel >> 8) & 0xff] | bl[pixel & 0xff];
		}
	}
}

/***************************************************************************
    Video start / update
***************************************************************************/

void namcos22_video_reset(void)
{
	node_used = 0;
	memset(&scenenode_root, 0, sizeof(scenenode_root));
	posirq_update();
}

static int namcos22_common_video_start(void)
{
	const UINT8 *tmap = memory_region(REGION_GFX3);
	const UINT8 *packed;
	int i, attr, y, x;

	pens = auto_malloc(0x8000 * sizeof(UINT32));
	dirtypal = auto_malloc(0x8000 / 4);
	mix_bitmap = auto_malloc(SCREEN_W * SCREEN_H * sizeof(UINT16));
	primap = auto_malloc(SCREEN_W * SCREEN_H);
	frame = auto_malloc(SCREEN_W * SCREEN_H * sizeof(UINT32));
	if (!pens || !dirtypal || !mix_bitmap || !primap || !frame)
		return 1;

	memset(pens, 0, 0x8000 * sizeof(UINT32));
	memset(dirtypal, 1, 0x8000 / 4);
	memset(mix_bitmap, 0, SCREEN_W * SCREEN_H * sizeof(UINT16));

	matrix3d_identity(viewmatrix);

	/* textures: the tilemap is little endian words, then packed attribute nibbles */
	texture_tilemap = auto_malloc(0x100000 * sizeof(UINT16));
	texture_tileattr = auto_malloc(0x100000);
	if (!texture_tilemap || !texture_tileattr)
		return 1;
	for (i = 0; i < 0x100000; i++)
		texture_tilemap[i] = tmap[i * 2] | (tmap[i * 2 + 1] << 8);
	packed = tmap + 0x200000;
	for (i = 0; i < 0x80000; i++)
	{
		texture_tileattr[i * 2] = packed[i] >> 4;
		texture_tileattr[i * 2 + 1] = packed[i] & 0xf;
	}
	texture_tiledata = memory_region(REGION_GFX2);

	for (attr = 0; attr < 16; attr++)
		for (y = 0; y < 16; y++)
			for (x = 0; x < 16; x++)
			{
				int ix = x, iy = y;
				if (attr & 4) ix = 15 - ix;
				if (attr & 2) iy = 15 - iy;
				if (attr & 8) { int temp = ix; ix = iy; iy = temp; }
				texture_ayx_to_pixel[(attr << 8) | (y << 4) | x] = (iy << 4) | ix;
			}

	/* System 22 specific */
	switch (namcos22_gametype)
	{
		case NAMCOS22_RIDGE_RACER:
		case NAMCOS22_RIDGE_RACER2:
		case NAMCOS22_ACE_DRIVER:
		case NAMCOS22_CYBER_COMMANDO:
			for (i = 0; i < 0x100000; i++)
			{
				int tile = texture_tilemap[i];
				if (~texture_tileattr[i] & 1)
					texture_tilemap[i] = (tile & 0x3fff) | 0x8000;
			}
			break;
	}

	texture_tile32 = auto_malloc(0x100000 * sizeof(UINT32));
	if (!texture_tile32)
		return 1;
	for (i = 0; i < 0x100000; i++)
		texture_tile32[i] = ((UINT32)texture_tilemap[i] << 12) | texture_tileattr[i];

	posirq_timer = timer_alloc(posirq_callback);

	state_save_register_UINT8("namcos22v", 0, "dirtypal", dirtypal, 0x8000 / 4);
	state_save_register_UINT16("namcos22v", 0, "tilemapattr", tilemapattr, 8);
	state_save_register_UINT16("namcos22v", 0, "rowscroll", rowscroll, 480);
	state_save_register_UINT16("namcos22v", 0, "lastrow", &lastrow, 1);
	state_save_register_UINT32("namcos22v", 0, "rs_frame", &rs_frame, 1);
	state_save_register_int("namcos22v", 0, "absolute_priority", &absolute_priority);
	state_save_register_int("namcos22v", 0, "objectshift", &objectshift);
	state_save_register_int("namcos22v", 0, "cz_adjust", &cz_adjust);
	state_save_register_int("namcos22v", 0, "objectflags", &objectflags);
	state_save_register_float("namcos22v", 0, "viewmatrix", &viewmatrix[0][0], 16);
	state_save_register_UINT8("namcos22v", 0, "reflection", &reflection, 1);
	state_save_register_int("namcos22v", 0, "cullflip", &cullflip);
	state_save_register_int("namcos22v", 0, "camera_vx", &camera_vx);
	state_save_register_int("namcos22v", 0, "camera_vy", &camera_vy);
	state_save_register_int("namcos22v", 0, "camera_vu", &camera_vu);
	state_save_register_int("namcos22v", 0, "camera_vd", &camera_vd);
	state_save_register_int("namcos22v", 0, "camera_vl", &camera_vl);
	state_save_register_int("namcos22v", 0, "camera_vr", &camera_vr);
	state_save_register_float("namcos22v", 0, "camera_zoom", &camera_zoom, 1);
	state_save_register_float("namcos22v", 0, "camera_lx", &camera_lx, 1);
	state_save_register_float("namcos22v", 0, "camera_ly", &camera_ly, 1);
	state_save_register_float("namcos22v", 0, "camera_lz", &camera_lz, 1);
	state_save_register_int("namcos22v", 0, "camera_ambient", &camera_ambient);
	state_save_register_int("namcos22v", 0, "camera_power", &camera_power);
	return 0;
}

VIDEO_START( namcos22s )
{
	int bank;

	if (namcos22_common_video_start())
		return 1;

	sprite_data = memory_region(REGION_GFX1);
	sprite_count = memory_region_length(REGION_GFX1) / (32 * 32);

	spotram = auto_malloc(0x800 * sizeof(UINT16));
	if (!spotram)
		return 1;
	memset(spotram, 0, 0x800 * sizeof(UINT16));

	for (bank = 0; bank < 4; bank++)
	{
		banked_czram[bank] = auto_malloc(0x100 * sizeof(UINT16));
		recalc_czram[bank] = auto_malloc(0x2000);
		if (!banked_czram[bank] || !recalc_czram[bank])
			return 1;
		memset(banked_czram[bank], 0, 0x100 * sizeof(UINT16));
		memset(recalc_czram[bank], 0, 0x2000);
		cz_was_written[bank] = 1;
		state_save_register_UINT16("namcos22v", bank, "banked_czram", banked_czram[bank], 0x100);
	}

	state_save_register_UINT16("namcos22v", 0, "spotram", spotram, 0x800);
	state_save_register_UINT32("namcos22v", 0, "spotram_enable", &spotram_enable, 1);
	state_save_register_UINT32("namcos22v", 0, "spotram_address", &spotram_address, 1);
	state_save_register_int("namcos22v", 0, "chipselect", &chipselect);
	state_save_register_UINT16("namcos22v", 0, "czattr", czattr, 8);
	state_save_register_UINT32("namcos22v", 0, "cz_was_written", cz_was_written, 4);
	return 0;
}

/* the frame, row by row: the main thread prepares it, then each band (one
   thread, or several with the "Threaded Video" option) draws its rows from
   the background to the screen bitmap */
static struct
{
	struct mame_bitmap *bitmap;
	const struct rectangle *cliprect;
	UINT32 bg;
	int layer;
	int gamma_identity;
} fj;

static void ss22_frame_rows(int band, int nbands)
{
	int x, y;

	for (y = 0; y < SCREEN_H; y++)
		if (ROW_OWNED(y, band, nbands))
		{
			UINT32 *f = frame + y * SCREEN_W;
			for (x = 0; x < SCREEN_W; x++)
				f[x] = fj.bg;
			memset(primap + y * SCREEN_W, 0, SCREEN_W);
		}

	if (fj.layer & 4)
	{
		draw_text_tilemap(band, nbands, 4, 4);
		namcos22s_mix_text_layer(band, nbands, 4);
	}
	render_scene_band(band, nbands);
	if (fj.layer & 4)
		namcos22s_mix_text_layer(band, nbands, 6);

	/* gamma, to the screen */
	{
		const struct rectangle *cliprect = fj.cliprect;
		const int minx = cliprect->min_x, w = cliprect->max_x - cliprect->min_x + 1;
		const UINT32 *rp = gamma_lut[0], *gp = gamma_lut[1], *bp = gamma_lut[2];

		for (y = cliprect->min_y; y <= cliprect->max_y; y++)
		{
			const UINT32 *src = frame + y * SCREEN_W + minx;
			UINT32 *dst = (UINT32 *)fj.bitmap->line[y] + minx;
			int n;

			if (!ROW_OWNED(y, band, nbands))
				continue;
			if (fj.gamma_identity)
				memcpy(dst, src, w * sizeof(UINT32));
			else
				for (n = w; n > 0; n--)
				{
					UINT32 rgb = *src++;
					*dst++ = rp[(rgb >> 16) & 0xff] | gp[(rgb >> 8) & 0xff] | bp[rgb & 0xff];
				}
		}
	}
}

static void s22_frame_rows(int band, int nbands)
{
	int x, y;

	for (y = 0; y < SCREEN_H; y++)
		if (ROW_OWNED(y, band, nbands))
		{
			UINT32 *f = frame + y * SCREEN_W;
			for (x = 0; x < SCREEN_W; x++)
				f[x] = fj.bg;
			memset(primap + y * SCREEN_W, 0, SCREEN_W);
		}

	/* polygons, then the text layer and the final mix */
	render_scene_band(band, nbands);
	draw_text_tilemap(band, nbands, 2, 3);
	namcos22_mix_text_layer(band, nbands, fj.bitmap, fj.cliprect);
}

VIDEO_UPDATE( namcos22s )
{
	int bg_r, bg_g, bg_b, x;
	const UINT32 *m = namcos22_mixer;

	render_frame_active();
	update_mixer();
	update_palette();
	recalc_czram_tables();

	/* background color */
	bg_r = NTHBYTE32(m, 0x08);
	bg_g = NTHBYTE32(m, 0x09);
	bg_b = NTHBYTE32(m, 0x0a);
	if ((mixer_flags & 1) && screen_fade_factor)
		BLEND(bg_r, bg_g, bg_b, screen_fade_r, screen_fade_g, screen_fade_b, 0xff - screen_fade_factor);
	fj.bg = TO_RGB(bg_r, bg_g, bg_b);

	/* gamma */
	fj.gamma_identity = 1;
	for (x = 0; x < 256; x++)
	{
		gamma_lut[0][x] = NTHBYTE32(m, 0x100 + x) << 16;
		gamma_lut[1][x] = NTHBYTE32(m, 0x200 + x) << 8;
		gamma_lut[2][x] = NTHBYTE32(m, 0x300 + x);
		if (gamma_lut[0][x] != (UINT32)(x << 16) || gamma_lut[1][x] != (UINT32)(x << 8) || gamma_lut[2][x] != (UINT32)x)
			fj.gamma_identity = 0;
	}

	/* the scene, then the rows */
	fj.layer = NTHBYTE32(m, 0x1f);
	fj.bitmap = bitmap;
	fj.cliprect = cliprect;
	if (fj.layer & 4)
		prepare_text_tilemap();
	if (fj.layer & 2) draw_sprites();
	if (fj.layer & 1) draw_polygons();
	render_frame(ss22_frame_rows);
}

VIDEO_START( namcos22 )
{
	if (namcos22_common_video_start())
		return 1;
	namcos22_gamma_proms = memory_region(REGION_PROMS);
	sprite_data = NULL;
	sprite_count = 1;
	return 0;
}

VIDEO_UPDATE( namcos22 )
{
	int i;

	render_frame_active();
	update_mixer();
	update_palette();

	fj.bg = pens[bg_palbase | 0xff];
	for (i = 0; i < 256; i++)
	{
		gamma_lut[0][i] = namcos22_gamma_proms[0x000 + i] << 16;
		gamma_lut[1][i] = namcos22_gamma_proms[0x100 + i] << 8;
		gamma_lut[2][i] = namcos22_gamma_proms[0x200 + i];
	}
	fj.bitmap = bitmap;
	fj.cliprect = cliprect;

	draw_polygons();
	prepare_text_tilemap();
	render_frame(s22_frame_rows);
}

VIDEO_STOP( namcos22 )
{
#ifdef HAVE_THREADS
	s22_threads_stop();
#endif
	{
		int i;
		for (i = 0; i < node_chunk_count; i++)
		{
			free(node_chunks[i]);
			node_chunks[i] = NULL;
		}
		node_chunk_count = 0;
		node_used = 0;
	}
}
