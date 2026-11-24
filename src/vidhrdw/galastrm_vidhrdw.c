/*

Differences from MAME:
- The TC0610 rotation polygon is drawn with the legacy triangle rasterizer
  (poly.c) with integer vertices, instead of the poly manager.

*/

#include "driver.h"
#include <math.h>
#include "vidhrdw/generic.h"
#include "vidhrdw/taitoic.h"
#include "vidhrdw/poly.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEGREE_TO_RADIAN(x) (M_PI * (x) / 180.0)


#define TC0100SCN_GFX_NUM 0
#define TC0480SCP_GFX_NUM 1
#define X_OFFSET 96
#define Y_OFFSET 60

struct tempsprite
{
	int gfx;
	int code,color;
	int flipx,flipy;
	int x,y;
	int zoomx,zoomy;
	int primask;
};
static struct tempsprite *spritelist;
static struct tempsprite *sprite_ptr_pre;
static struct mame_bitmap *tmpbitmaps;
static struct mame_bitmap *polybitmap;
INT16 galastrm_tc0610_ctrl_reg[2][8];

/******************************************************************/

VIDEO_START( galastrm )
{
	spritelist = auto_malloc(0x4000 * sizeof(*spritelist));

	if (TC0100SCN_vh_start(1,TC0100SCN_GFX_NUM,48-X_OFFSET,4-Y_OFFSET,0,0,0,0,0))
		return 1;

	if (TC0480SCP_vh_start(TC0480SCP_GFX_NUM,0,56-X_OFFSET,-63+Y_OFFSET,0,0,0,0,0))
		return 1;

	tmpbitmaps = auto_bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);
	polybitmap = auto_bitmap_alloc(Machine->drv->screen_width,Machine->drv->screen_height);

	if (!tmpbitmaps || !polybitmap)
		return 1;

	return 0;
}

/************************************************************
            SPRITE DRAW ROUTINES
********************************************************/

static void draw_sprites_pre(int x_offs, int y_offs)
{
	UINT16 *spritemap = (UINT16 *)memory_region(REGION_USER1);
	int offs, data, tilenum, color, flipx, flipy;
	int x, y, priority, dblsize, curx, cury;
	int zoomx, zoomy, zx, zy;
	int sprite_chunk,map_offset,code,j,k,px,py;
	int dimension,total_chunks;

	/* pdrawgfx() needs us to draw sprites front to back, so we have to build a list
       while processing sprite ram and then draw them all at the end */
	sprite_ptr_pre = spritelist;

	for (offs = (spriteram_size/4-4);offs >= 0;offs -= 4)
	{
		data = spriteram32[offs+0];
		flipx =    (data & 0x00800000) >> 23;
		zoomx =    (data & 0x007f0000) >> 16;
		tilenum =  (data & 0x00007fff);

		if (!tilenum) continue;

		data = spriteram32[offs+2];
		priority = (data & 0x000c0000) >> 18;
		color =    (data & 0x0003fc00) >> 10;
		x =        (data & 0x000003ff);

		data = spriteram32[offs+3];
		dblsize =  (data & 0x00040000) >> 18;
		flipy =    (data & 0x00020000) >> 17;
		zoomy =    (data & 0x0001fc00) >> 10;
		y =        (data & 0x000003ff);

		dimension = ((dblsize*2) + 2);	/* 2 or 4 */
		total_chunks = ((dblsize*3) + 1) << 2;	/* 4 or 16 */
		map_offset = tilenum << 2;

		zoomx += 1;
		zoomy += 1;

		if (x > 713) x -= 1024;		/* 1024x512 */
		if (y < 117) y += 512;

		y = (-y & 0x3ff);
		x -= x_offs;
		y += y_offs;
		if (flipy) y += (128 - zoomy);

		for (sprite_chunk=0;sprite_chunk<total_chunks;sprite_chunk++)
		{
			j = sprite_chunk / dimension;   /* rows */
			k = sprite_chunk % dimension;   /* chunks per row */

			px = k;
			py = j;
			/* pick tiles back to front for x and y flips */
			if (flipx)  px = dimension-1-k;
			if (flipy)  py = dimension-1-j;

			code = spritemap[map_offset + px + (py<<(dblsize+1))];

			/* unused portions of the spritemap rom contain 0xffff */
			if (code==0xffff)
				continue;

			curx = x + ((k*zoomx)/dimension);
			cury = y + ((j*zoomy)/dimension);

			zx= x + (((k+1)*zoomx)/dimension) - curx;
			zy= y + (((j+1)*zoomy)/dimension) - cury;

			sprite_ptr_pre->gfx = 0;
			sprite_ptr_pre->code = code;
			sprite_ptr_pre->color = color;
			sprite_ptr_pre->flipx = !flipx;
			sprite_ptr_pre->flipy = flipy;
			sprite_ptr_pre->x = curx;
			sprite_ptr_pre->y = cury;
			sprite_ptr_pre->zoomx = zx << 12;
			sprite_ptr_pre->zoomy = zy << 12;
			sprite_ptr_pre->primask = priority;

			sprite_ptr_pre++;
		}
	}
}

static void draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect,const int *primasks,int priority)
{
	struct tempsprite *sprite_ptr = sprite_ptr_pre;

	while (sprite_ptr != spritelist)
	{
		sprite_ptr--;

		if ((priority != 0 && sprite_ptr->primask != 0) ||
			(priority == 0 && sprite_ptr->primask == 0))
		{
			pdrawgfxzoom(bitmap,Machine->gfx[sprite_ptr->gfx],
					sprite_ptr->code,
					sprite_ptr->color,
					sprite_ptr->flipx,sprite_ptr->flipy,
					sprite_ptr->x,sprite_ptr->y,
					cliprect,TRANSPARENCY_PEN,0,
					sprite_ptr->zoomx,sprite_ptr->zoomy,
					primasks[sprite_ptr->primask]);
		}
	}
}

/**************************************************************
                POLYGON RENDERER
**************************************************************/

static void tc0610_draw_triangle(struct mame_bitmap *bitmap, struct mame_bitmap *srcbitmap, const struct rectangle *clip, const struct poly_vertex *v0, const struct poly_vertex *v1, const struct poly_vertex *v2)
{
	const struct poly_scanline_data *scans;
	const struct poly_scanline *curscan;
	int maxu = srcbitmap->width - 1;
	int maxv = srcbitmap->height - 1;
	int x, y;

	scans = setup_triangle_2(v0, v1, v2, clip);
	if (!scans)
		return;

	curscan = scans->scanline;
	for (y = scans->sy; y <= scans->ey; y++, curscan++)
	{
		UINT16 *d = (UINT16 *)bitmap->line[y];
		int width = curscan->ex - curscan->sx + 1;
		INT64 u = curscan->p[0];
		INT64 v = curscan->p[1];

		for (x = 0; x < width; x++)
		{
			int tu = u >> 16;
			int tv = v >> 16;

			/* the interpolation of the integer vertices can slightly overshoot the texture */
			if (tu < 0) tu = 0;
			if (tu > maxu) tu = maxu;
			if (tv < 0) tv = 0;
			if (tv > maxv) tv = maxv;

			d[curscan->sx + x] = ((UINT16 *)srcbitmap->line[tv])[tu];
			u += scans->dp[0];
			v += scans->dp[1];
		}
	}
}

static void tc0610_rotate_draw(struct mame_bitmap *bitmap, struct mame_bitmap *srcbitmap, const struct rectangle *clip)
{
	struct poly_vertex vert[4];
	int rsx = galastrm_tc0610_ctrl_reg[1][0];
	int rsy = galastrm_tc0610_ctrl_reg[1][1];
	const int rzx = galastrm_tc0610_ctrl_reg[1][2];
	const int rzy = galastrm_tc0610_ctrl_reg[1][3];
	const int ryx = galastrm_tc0610_ctrl_reg[1][5];
	const int ryy = galastrm_tc0610_ctrl_reg[1][4];
	const int lx  = srcbitmap->width;
	const int ly  = srcbitmap->height;

	static int rsxb=0, rsyb=0, rsxoffs=0, rsyoffs=0;
	int sx, sy, yx, yy, zx, zy, pxx, pxy, pyx, pyy;
	float ssn, scs, ysn, ycs, zsn, zcs;
	int i;

	pxx = 0;
	pxy = 0;
	pyx = 0;
	pyy = 0;
	zx  = 0;
	zy  = 0;

	if (rzx != 0 || rzy != 0)
	{
		while (sqrt(pow((float)pxx/4096.0, 2.0) + pow((float)pxy/4096.0, 2.0)) < (float)(lx / 2))
		{
			pxx += rzx;
			pxy += rzy;
			zx++;
		}
		while (sqrt(pow((float)pyy/4096.0, 2.0) + pow((float)pyx/4096.0, 2.0)) < (float)(ly / 2))
		{
			pyy += rzx;
			pyx += -rzy;
			zy++;
		}
	}
	zsn = ((float)pyx/4096.0) / (float)(ly / 2);
	zcs = ((float)pxx/4096.0) / (float)(lx / 2);

	if ((rsx == -240 && rsy == 1072) || !galastrm_tc0610_ctrl_reg[1][7])
	{
		rsxoffs = 0;
		rsyoffs = 0;
	}
	else
	{
		if (rsx > rsxb && rsxb < 0 && rsx-rsxb > 0x8000)
		{
			if (rsxoffs == 0)
				rsxoffs = -0x10000;
			else
				rsxoffs = 0;
		}
		if (rsx < rsxb && rsxb > 0 && rsxb-rsx > 0x8000)
		{
			if (rsxoffs == 0)
				rsxoffs = 0x10000-1;
			else
				rsxoffs = 0;
		}
		if (rsy > rsyb && rsyb < 0 && rsy-rsyb > 0x8000)
		{
			if (rsyoffs == 0)
				rsyoffs = -0x10000;
			else
				rsyoffs = 0;
		}
		if (rsy < rsyb && rsyb > 0 && rsyb-rsy > 0x8000)
		{
			if (rsyoffs == 0)
				rsyoffs = 0x10000-1;
			else
				rsyoffs = 0;
		}
	}
	rsxb = rsx;
	rsyb = rsy;
	if (rsxoffs) rsx += rsxoffs;
	if (rsyoffs) rsy += rsyoffs;
	if (rsx < -0x14000 || rsx >= 0x14000) rsxoffs = 0;
	if (rsy < -0x14000 || rsy >= 0x14000) rsyoffs = 0;

	pxx = 0;
	pxy = 0;
	pyx = 0;
	pyy = 0;
	sx  = 0;
	sy  = 0;
	yx  = 0;
	yy  = 0;
	ssn = 0.0;
	scs = 0.0;
	ysn = 0.0;
	ycs = 0.0;

	if (galastrm_tc0610_ctrl_reg[1][7])
	{
		if (ryx != 0 || ryy != 0)
		{
			while (sqrt(pow((float)pxx/4096.0, 2.0) + pow((float)pxy/4096.0, 2.0)) < (float)(lx / 2))
			{
				pxx += ryx;
				pxy += ryy;
				yx++;
			}
			while (sqrt(pow((float)pyy/4096.0, 2.0) + pow((float)pyx/4096.0, 2.0)) < (float)(ly / 2))
			{
				pyy += ryx;
				pyx += -ryy;
				yy++;
			}
			if (yx >= 0.0)
			{
				yx = (int)((8.0 - log((double)yx) / log(2.0)) * 6.0);
				ysn = sin(DEGREE_TO_RADIAN(yx));
				ycs = 1.0 - ysn*ysn;
			}
		}

		pxx = 0;
		pxy = 0;
		pyx = 0;
		pyy = 0;

		if (rsx != 0 || rsy != 0)
		{
			while (sqrt(pow((float)pxx/65536.0, 2.0) + pow((float)pxy/65536.0, 2.0)) < (float)(lx / 2))
			{
				pxx += rsx;
				pxy += rsy;
				sx++;
			}
			while (sqrt(pow((float)pyy/65536.0, 2.0) + pow((float)pyx/65536.0, 2.0)) < (float)(ly / 2))
			{
				pyy += rsx;
				pyx += -rsy;
				sy++;
			}
		}
		ssn = ((float)pxy/65536.0) / (float)(lx / 2);
		scs = ((float)pyy/65536.0) / (float)(ly / 2);
	}

	{
		float cx[4], cy[4];

		cx[0] = -zx;  cy[0] = -zy;
		cx[1] = -zx;  cy[1] = zy-1;
		cx[2] = zx-1; cy[2] = zy-1;
		cx[3] = zx-1; cy[3] = -zy;

		for (i = 0; i < 4; i++)
		{
			vert[i].x = (INT32)((cx[i] * zcs) - (cy[i] * zsn) + (float)(lx / 2));
			vert[i].y = (INT32)((cx[i] * zsn) + (cy[i] * zcs) + (float)(ly / 2));
		}
	}

	vert[0].p[0] = 0;
	vert[0].p[1] = 0;
	vert[1].p[0] = 0;
	vert[1].p[1] = ly - 1;
	vert[2].p[0] = lx - 1;
	vert[2].p[1] = ly - 1;
	vert[3].p[0] = lx - 1;
	vert[3].p[1] = 0;

	/* the quad is drawn as two triangles: 0,1,2 and 0,3,2 */
	tc0610_draw_triangle(bitmap, srcbitmap, clip, &vert[0], &vert[1], &vert[2]);
	tc0610_draw_triangle(bitmap, srcbitmap, clip, &vert[0], &vert[3], &vert[2]);
}

/**************************************************************
                SCREEN REFRESH
**************************************************************/

VIDEO_UPDATE( galastrm )
{
	UINT8 layer[5];
	UINT8 pivlayer[3];
	UINT16 priority;
	static const int primasks[4] = {0xfffc, 0xfff0, 0xff00, 0x0};
	struct rectangle clip;

	clip.min_x = 0;
	clip.min_y = 0;
	clip.max_x = tmpbitmaps->width - 1;
	clip.max_y = tmpbitmaps->height - 1;

	TC0100SCN_tilemap_update();
	TC0480SCP_tilemap_update();

	priority = TC0480SCP_get_bg_priority();
	layer[0] = (priority &0xf000) >> 12;	/* tells us which bg layer is bottom */
	layer[1] = (priority &0x0f00) >>  8;
	layer[2] = (priority &0x00f0) >>  4;
	layer[3] = (priority &0x000f) >>  0;	/* tells us which is top */
	layer[4] = 4;   /* text layer always over bg layers */

	pivlayer[0] = TC0100SCN_bottomlayer(0);
	pivlayer[1] = pivlayer[0]^1;
	pivlayer[2] = 2;

	fillbitmap(bitmap,Machine->pens[0],cliprect);
	fillbitmap(priority_bitmap,0,&clip);
	fillbitmap(tmpbitmaps,Machine->pens[0],&clip);

	TC0100SCN_tilemap_draw(bitmap,cliprect,0,pivlayer[0],0,0);
	TC0100SCN_tilemap_draw(bitmap,cliprect,0,pivlayer[1],0,0);

	if (layer[0]==0 && layer[1]==3 && layer[2]==2 && layer[3]==1)
	{
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[0],0,1);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[1],0,4);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[2],0,4);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[3],0,4);
	}
	else
	{
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[0],0,1);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[1],0,2);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[2],0,4);
		TC0480SCP_tilemap_draw(tmpbitmaps,&clip,layer[3],0,8);
	}

	if (layer[0]==3 && layer[1]==0 && layer[2]==1 && layer[3]==2)
	{
		int x,y;
		UINT8 *pri;

		for (y=0; y < priority_bitmap->height; y++)
		{
			for (x=0; x < priority_bitmap->width; x++)
			{
				pri = &((UINT8 *)priority_bitmap->line[y])[x];
				if (!(*pri & 0x02) && ((UINT16 *)tmpbitmaps->line[y])[x] != Machine->pens[0])
					*pri |= 0x04;
			}
		}
	}

	draw_sprites_pre(42-X_OFFSET, -571+Y_OFFSET);
	draw_sprites(tmpbitmaps,&clip,primasks,1);

	copybitmap(bitmap,polybitmap,0,0,0,0,cliprect,TRANSPARENCY_PEN,Machine->pens[0]);
	fillbitmap(polybitmap,Machine->pens[0],&clip);
	tc0610_rotate_draw(polybitmap,tmpbitmaps,cliprect);

	fillbitmap(priority_bitmap,0,cliprect);
	draw_sprites(bitmap,cliprect,primasks,0);

	TC0480SCP_tilemap_draw(bitmap,cliprect,layer[4],0,0);
	TC0100SCN_tilemap_draw(bitmap,cliprect,0,pivlayer[2],0,0);
}
