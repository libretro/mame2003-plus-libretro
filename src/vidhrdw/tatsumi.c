#include "driver.h"
#include "vidhrdw/generic.h"
#include "tatsumi.h"
#include <math.h>

/*static struct tilemap *bg_layer,*tx_layer; */
static struct tilemap *tx_layer;
static struct tilemap *layer0, *layer1, *layer2, *layer3;
static struct mame_bitmap *temp_bitmap;

data16_t *roundup_r_ram, *roundup_p_ram, *roundup_l_ram;
data16_t *cyclwarr_videoram0, *cyclwarr_videoram1;
data16_t* tatsumi_sprite_control_ram;
data8_t* roundup5_vram;

extern UINT16 bigfight_a20000[8], bigfight_a40000[2], bigfight_a60000[8];
UINT16 bigfight_bank, bigfight_last_bank;

static data8_t roundupt_crt_selected_reg;
static data8_t roundupt_crt_reg[64];
extern data16_t debugA,debugB,debugC,debugD;

static data8_t* shadow_pen_array;

/******************************************************************************/

WRITE16_HANDLER(tatsumi_sprite_control_w)
{
	COMBINE_DATA(&tatsumi_sprite_control_ram[offset]);

	/* 0xe0 is bank switch, others unknown */
/*	if ((offset==0xe0 && data&0xefff) || offset!=0xe0) */
/*		logerror("%08x:  Tatsumi TZB215 sprite control %04x %08x\n", activecpu_get_pc(), offset, data); */
}

/******************************************************************************/

READ_HANDLER(roundup5_vram_r)
{
	offset+=((tatsumi_control_word&0x0c00)>>10) * 0x10000;
	return roundup5_vram[offset];
}

WRITE_HANDLER(roundup5_vram_w)
{
	offset+=((tatsumi_control_word&0x0c00)>>10) * 0x18000;

/*	if (offset>=0x30000) */
/*		logerror("effective write to vram %06x %02x (control %04x)\n",offset,data,tatsumi_control_word); */

	roundup5_vram[offset]=data;

	offset=offset%0x18000;

	decodechar(Machine->gfx[1],offset/0x10,(unsigned char *)roundup5_vram,
			Machine->drv->gfxdecodeinfo[1].gfxlayout);
}


WRITE_HANDLER(roundup5_palette_w)
{
/*	static int hack=0; */
	int r,g,b,word;

	paletteram[offset]=data;

/*	if (offset==0xbfe) */
/*		hack++; */

/*	if (hack>1) */
/*		return; */

/*
apache 3 schematics state

bit 4:  250
bit 3:  500
bit 2:  1k
bit 1:  2k
bit 0:  3.9kOhm resistor

*/

/*	logerror("PAL: %04x %02x\n",offset,data); */

	offset&=~3;
	word=(paletteram[offset]<<8)|(paletteram[offset+2]);
	r = ((word >>10) & 0x1f)*8;
	g = ((word >> 5) & 0x1f)*8;
	b = ((word >> 0) & 0x1f)*8;

	palette_set_color(offset/4,r,g,b);
}
WRITE_HANDLER(apache3_palette_w)
{
/*	static int hack=0; */
	int r,g,b,word;

	paletteram[offset]=data;

/*	if (offset==0xbfe) */
/*		hack++; */

/*	if (hack>1) */
/*		return; */

/*
apache 3 schematics state

bit 4:  250
bit 3:  500
bit 2:  1k
bit 1:  2k
bit 0:  3.9kOhm resistor

*/

/*	logerror("PAL: %04x %02x\n",offset,data); */

	offset&=~1;
	word=(paletteram[offset+1]<<8)|(paletteram[offset]);
	r = ((word >>10) & 0x1f)*8;
	g = ((word >> 5) & 0x1f)*8;
	b = ((word >> 0) & 0x1f)*8;

	palette_set_color(offset/2,r,g,b);
}


WRITE_HANDLER( roundup5_text_w )
{
	videoram[offset]=data;
	tilemap_mark_tile_dirty( tx_layer,offset/2);
}

READ16_HANDLER( cyclwarr_videoram0_r )
{
	 return cyclwarr_videoram0[offset];
}

READ16_HANDLER( cyclwarr_videoram1_r )
{
	 return cyclwarr_videoram1[offset];
}

WRITE16_HANDLER( cyclwarr_videoram0_w )
{
    COMBINE_DATA(&cyclwarr_videoram0[offset]);
	if (offset>=0x400)
	{
		tilemap_mark_tile_dirty( layer0, offset-0x400);
		tilemap_mark_tile_dirty( layer1, offset-0x400);
	}
}

WRITE16_HANDLER( cyclwarr_videoram1_w )
{
    COMBINE_DATA(&cyclwarr_videoram1[offset]);
	if (offset>=0x400)
	{
		tilemap_mark_tile_dirty( layer2, offset-0x400);
		tilemap_mark_tile_dirty( layer3, offset-0x400);
	}
}

WRITE_HANDLER( roundup5_crt_w )
{
	if (offset==0)
		roundupt_crt_selected_reg=data&0x3f;
	if (offset==2) {
		roundupt_crt_reg[roundupt_crt_selected_reg]=data;
/*		if (roundupt_crt_selected_reg!=0xa && roundupt_crt_selected_reg!=0xb && roundupt_crt_selected_reg!=29) */
/*		logerror("%08x:  Crt write %02x %02x\n",activecpu_get_pc(),roundupt_crt_selected_reg,data); */
	}
}

/********************************************************************/

static void get_text_tile_info(int tile_index)
{
	int tile=videoram[2*tile_index]+((videoram[2*tile_index+1]&0xf)<<8);
	int color=videoram[2*tile_index+1]>>4;

	SET_TILE_INFO(
			1,
			tile,
			color,
			0)
}

static void get_tile_info_bigfight_0(int tile_index)
{
    int tile=cyclwarr_videoram0[(tile_index+0x400)%0x8000];
	int bank = (bigfight_a40000[0] >> (((tile&0xc00)>>10)*4))&0xf;
	SET_TILE_INFO(1,(tile&0x3ff)+(bank<<10),(tile>>12)&0xf,0);
}

static void get_tile_info_bigfight_1(int tile_index)
{
    int tile=cyclwarr_videoram1[(tile_index+0x400)%0x8000];
	int bank = (bigfight_a40000[0] >> (((tile&0xc00)>>10)*4))&0xf;
	SET_TILE_INFO(1,(tile&0x3ff)+(bank<<10),(tile>>12)&0xf,0);
}

/********************************************************************/

VIDEO_START( apache3 )
{
	tx_layer = tilemap_create(get_text_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,64);
	shadow_pen_array = auto_malloc(8192);
	temp_bitmap = bitmap_alloc_depth(512, 512, 32);

	if (!tx_layer || !shadow_pen_array || !temp_bitmap)
		return 1;

	memset(shadow_pen_array, 0, 8192);
	tilemap_set_transparent_pen(tx_layer,0);
	return 0;
}

VIDEO_START( roundup5 )
{
	tx_layer = tilemap_create(get_text_tile_info,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,64);
	shadow_pen_array = auto_malloc(8192);
	roundup5_vram = auto_malloc(0x48000 * 4);

	if (!tx_layer || !shadow_pen_array || !roundup5_vram)
		return 1;

	memset(shadow_pen_array, 0, 8192);
	tilemap_set_transparent_pen(tx_layer,0);
	return 0;
}

VIDEO_START( cyclwarr )
{
    layer0 = tilemap_create(get_tile_info_bigfight_0,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,512);
	/*layer1 = tilemap_create(get_tile_info_bigfight_0,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,512); */
	layer1 = tilemap_create(get_tile_info_bigfight_0,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,256);
	layer2 = tilemap_create(get_tile_info_bigfight_1,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,512);
	layer3 = tilemap_create(get_tile_info_bigfight_1,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,64,512);
	shadow_pen_array = auto_malloc(8192);

	if (!layer0 || !layer1 || !layer2 || !layer3 || !shadow_pen_array)
		return 1;

	memset(shadow_pen_array, 0, 8192);
	return 0;
}

VIDEO_START( bigfight )
{
	layer0 = tilemap_create(get_tile_info_bigfight_0,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,256);
	layer1 = tilemap_create(get_tile_info_bigfight_0,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,256);
	layer2 = tilemap_create(get_tile_info_bigfight_1,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,256);
	layer3 = tilemap_create(get_tile_info_bigfight_1,tilemap_scan_rows,TILEMAP_TRANSPARENT,8,8,128,256);
	shadow_pen_array = auto_malloc(8192);

	if (!layer0 || !layer1 || !layer2 || !layer3 || !shadow_pen_array)
		return 1;

	memset(shadow_pen_array, 0, 8192);
	return 0;
}

/********************************************************************/

INLINE void roundupt_drawgfxzoomrotate( struct mame_bitmap *dest_bmp,const struct GfxElement *gfx,
		unsigned int code,unsigned int color,int flipx,int flipy,unsigned int ssx,unsigned int ssy,
		const struct rectangle *clip, int scalex, int scaley, int rotate, int write_priority_only )
{
	struct rectangle myclip;

	if (!scalex || !scaley) return;

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

	{
		if( gfx && gfx->colortable )
		{
			const pen_t *pal = &gfx->colortable[gfx->color_granularity * (color % gfx->total_colors)];
			const data8_t *shadow_pens = shadow_pen_array + (gfx->color_granularity * (color % gfx->total_colors));
			int source_base = (code % gfx->total_elements) * gfx->height;

			int block_size = 8 * scalex;
			int sprite_screen_height = ((ssy&0xffff)+block_size)>>16;
			int sprite_screen_width = ((ssx&0xffff)+block_size)>>16;

			if (sprite_screen_width && sprite_screen_height)
			{
				/* compute sprite increment per screen pixel */
				int dx = (gfx->width<<16)/sprite_screen_width;
				int dy = (gfx->height<<16)/sprite_screen_height;

				int sx;/*=ssx>>16; */
				int sy;/*=ssy>>16; */


/*				int ex = sx+sprite_screen_width; */
/*				int ey = sy+sprite_screen_height; */

				int x_index_base;
				int y_index;
				int ex,ey;

							int incxx=0x10000;/*(int)((float)dx * cos(theta)); */
/*							int incxy=0x0;*/ /*(int)((float)dy * -sin(theta)); */
							int incyx=0x0;/*(int)((float)dx * sin(theta)); */
/*							int incyy=0x10000;*/ /*(int)((float)dy * cos(theta)); */ 

							if (flipx)
							{

							}


				if (ssx&0x80000000) sx=0-(0x10000 - (ssx>>16)); else sx=ssx>>16;
				if (ssy&0x80000000) sy=0-(0x10000 - (ssy>>16)); else sy=ssy>>16;
				ex = sx+sprite_screen_width;
				ey = sy+sprite_screen_height;
				if( flipx )
				{
					x_index_base = (sprite_screen_width-1)*dx;
					dx = -dx;
					incxx=-incxx;
					incyx=-incyx;
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
#if 0
					/* case 1: TRANSPARENCY_PEN */
					if (transparency == TRANSPARENCY_PEN)
					{
						{
							int startx=0;
							int starty=0;

/*							int incxx=0x10000; */
/*							int incxy=0; */
/*							int incyx=0; */
/*							int incyy=0x10000; */
							double theta=rotate * ((2.0 * PI)/512.0);
							double c=cos(theta);
							double s=sin(theta);


						/*	if (ey-sy > 0) */
						/*		dy=dy / (ey-sy); */
							{
							float angleAsRadians=(float)rotate * (7.28f / 512.0f);
							/*float ccx = cosf(angleAsRadians); */
							/*float ccy = sinf(angleAsRadians); */
							float a=0;

							}

							for( y=sy; y<ey; y++ )
							{
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								int cx = startx;
								int cy = starty;

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									UINT8 *source = gfx->gfxdata + (source_base+((cy>>16))) * gfx->line_modulo;
									int c = source[(cx >> 16)];
									if( c != transparent_color )
									{
										if (write_priority_only)
											dest[x]=shadow_pens[c];
										else
											dest[x]=pal[c];
									}
									cx += incxx;
									cy += incxy;
								}
								startx += incyx;
								starty += incyy;
							}
						}
					}
#endif
#if 1 /* old */
					/*if (transparency == TRANSPARENCY_PEN) */
					{
						{
							for( y=sy; y<ey; y++ )
							{
								UINT8 *source = gfx->gfxdata + (source_base+(y_index>>16)) * gfx->line_modulo;
								UINT32 *dest = (UINT32 *)dest_bmp->line[y];
								UINT8 *priority_dest = (UINT8 *)dest_bmp->line[y];

								int x, x_index = x_index_base;
								for( x=sx; x<ex; x++ )
								{
									int c = source[x_index>>16];
									if( c )
									{
										/* Only draw shadow pens if writing priority buffer */
										if (write_priority_only)
											priority_dest[x]=shadow_pens[c];
										else if (!shadow_pens[c])
											dest[x]=pal[c];
									}
									x_index += dx;
								}

								y_index += dy;
							}
						}
					}
#endif
				}
			}
		}
	}
}

void mycopyrozbitmap_core(struct mame_bitmap *bitmap,struct mame_bitmap *srcbitmap,
		int dstx,int dsty, int srcwidth, int srcheight,int incxx,int incxy,int incyx,int incyy,
		const struct rectangle *clip,int transparency,int transparent_color)
{
	UINT32 cx;
	UINT32 cy;
	int x;
	int sx;
	int sy;
	int ex;
	int ey;
/*	const int xmask = srcbitmap->width-1; */
/*	const int ymask = srcbitmap->height-1; */
	const int widthshifted = srcwidth << 16;
	const int heightshifted = srcheight << 16;
	UINT32 *dest;

	UINT32 startx=0;
	UINT32 starty=0;

	sx = dstx;
	sy = dsty;
	ex = dstx + srcwidth;
	ey = dsty + srcheight;

	if (sx<clip->min_x) sx=clip->min_x;
	if (ex>clip->max_x) ex=clip->max_x;
	if (sy<clip->min_y) sy=clip->min_y;
	if (ey>clip->max_y) ey=clip->max_y;

	if (sx <= ex)
	{

		while (sy <= ey)
		{
			x = sx;
			cx = startx;
			cy = starty;
			dest = ((UINT32 *)bitmap->line[sy]) + sx;

			while (x <= ex)
			{
				if (cx < widthshifted && cy < heightshifted)
				{
					int c = ((UINT32 *)srcbitmap->line[cy >> 16])[cx >> 16];

					if (c != transparent_color)
						*dest = c;
				}

				cx += incxx;
				cy += incxy;
				x++;
				dest++;
			}
			startx += incyx;
			starty += incyy;
			sy++;
		}
	}
}

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int write_priority_only, int rambank)
{
	int offs,fx,x,y,color;
	int w,h,index,lines,scale,rotate;
	data8_t *src1, *src2;

	int y_offset;

	int render_x, render_y;
	int extent_x, extent_y;

	/* Sprite data is double buffered */
	for (offs = rambank;offs < rambank + 0x800;offs += 6)
	{
		/*
			Sprite RAM itself uses an index into two ROM tables to actually draw the object.

			Sprite RAM format:

			Word 0: 0xf000 - ?
					0x0fff - Index into ROM sprite table
			Word 1: 0x8000 - X Flip
					0x7ff8 - Colour (TODO:  Check top bit on Apache 3/Round up)
			        0x0007 - ?
			Word 2: 0xffff - X position
			Word 3: 0xffff - Y position
			Word 4: 0x01ff - Rotation

			Sprite ROM table format, alternate lines come from each bank, with the
			very first line indicating control information:

			First bank:
			Byte 0: Y destination offset (in scanlines, unaffected by scale).
			Byte 1: Always 0?
			Byte 2: Number of source scanlines to render from (so unaffected by destination scale).
			Byte 3: Usually 0, sometimes 0x80??

			Other banks:
			Byte 0: Width of line in tiles (-1)
			Byte 1: X offset to start drawing line at (multipled by scale * 8)
			Bytes 2/3: Tile index to start fetching tiles from (increments per tile).

		*/
		y = spriteram16[offs+3];
		x = spriteram16[offs+2];
		scale = spriteram16[offs+4]&0x1ff;
		color = (spriteram16[offs+1]>>3)&0x1ff;
		fx = spriteram16[offs+1]&0x8000;
		rotate = 0;/*spriteram16[offs+5]&0x1ff;*/ /* Todo:  Turned off for now */

		index = spriteram16[offs];

/*		if (spriteram16[offs+1]&0x7) */
/*			color=rand()%0xff; */

		/* End of sprite list marker */
		if (index==0xffff || spriteram16[offs+4]==0xffff) /*todo */
			return;

		if (index>=0x4000)
			continue;

		src1 = tatsumi_rom_sprite_lookup1 + (index*4);
		src2 = tatsumi_rom_sprite_lookup2 + (index*4);

		lines=src1[2];
		y_offset=src1[0]&0xf8;
		lines-=y_offset;

		render_x=x<<16;
		render_y=y<<16;
		scale=scale<<9; /* 0x80 becomes 0x10000 */

		render_y+=y_offset * scale;

		if (rotate)
		{
			render_y=0;
			fillbitmap(temp_bitmap, 0, 0);
		}

extent_x=extent_y=0;

		src1+=4;
		h=0;
		while (lines>0) {
			int base, x_offs, x_width, x_pos, draw_this_line=1;
			int this_extent=0;

			/* Odd and even lines come from different banks */
			if (h&1) {
				x_width=src1[0]+1;
				x_offs=src1[1]*scale*8;
				base=src1[2] | (src1[3]<<8);
			}
			else {
				x_width=src2[0]+1;
				x_offs=src2[1]*scale*8;
				base=src2[2] | (src2[3]<<8);
			}

			if (draw_this_line) {
				base*=2;

				if (!rotate)
				{
					if (fx)
						x_pos=render_x-x_offs-scale*8;
					else
						x_pos=render_x+x_offs;
				}
				else
					x_pos=x_offs;

				for (w=0; w<x_width; w++) {
					if (rotate)
						roundupt_drawgfxzoomrotate(temp_bitmap,Machine->gfx[0],
								base,
								color,fx,0,x_pos,render_y,
								cliprect,scale,scale,0,write_priority_only);
					else
						roundupt_drawgfxzoomrotate(bitmap,Machine->gfx[0],
								base,
								color,fx,0,x_pos,render_y,
								cliprect,scale,scale,0,write_priority_only);
					base++;

					if (fx)
						x_pos-=scale*8;
					else
						x_pos+=scale*8;

					this_extent+=scale*8;
				}
				if (h&1)
					src1+=4;
				else
					src2+=4;

				if (this_extent > extent_x)
					extent_x=this_extent;
				this_extent=0;

				render_y+=8 * scale;
				extent_y+=8 * scale;
				h++;
				lines-=8;
			} else {
				h=32; /*hack */
			}
		}

		if (rotate)
		{
			double theta=rotate * ((2.0 * PI)/512.0);

			int incxx=(int)((float)65536.0 * cos(theta));
			int incxy=(int)((float)65536.0 * -sin(theta));
			int incyx=(int)((float)65536.0 * sin(theta));
			int incyy=(int)((float)65536.0 * cos(theta));

			extent_x=extent_x>>16;
			extent_y=extent_y>>16;
			if (extent_x>2 && extent_y>2)
			mycopyrozbitmap_core(bitmap, temp_bitmap, x/* + (extent_x/2)*/, y /*+ (extent_y/2)*/, extent_x, extent_y, incxx, incxy, incyx, incyy, cliprect,
				TRANSPARENCY_PEN, 0);
		}
	}
}

static void draw_sky(struct mame_bitmap *bitmap,const struct rectangle *cliprect, int palette_base, int start_offset)
{
	/* all todo */
	int x,y;

	if (start_offset&0x8000)
		start_offset=-(0x10000 - start_offset);

	start_offset=-start_offset;

start_offset-=48;
	for (y=0; y<256; y++) {
		for (x=0; x<320; x++) {
			int col=palette_base + y + start_offset;
			if (col<palette_base) col=palette_base;
			if (col>palette_base+127) col=palette_base+127;

			plot_pixel(bitmap,x,y,Machine->pens[col]);
		}
	}
}

static void draw_road(struct mame_bitmap *bitmap,const struct rectangle *cliprect,struct mame_bitmap *shadow_bitmap)
{
/*
0xf980 0x0008 0x8c80 0x4a00	- road right to below, width unknown (32 pixels guess)
0xfa80 0x0008 0x8c80 0x4a00	- road right to below, width unknown (32 pixels guess)

0xfb80 0x0008 0x8c80 0x4a00	- road in middle of screen, width unknown (32 pixels guess)

0xfc80 0x0008 0x8c80 0x4a00 - road width above to left, same width as above (ie, xpos - 32)
0xfd80 0x0008 0x8c80 0x4a00 - road width above to left, same width as above (ie, xpos - 32)
0xfe80 0x0008 0x8c80 0x4a00 - road width above to left, same width as above (ie, xpos - 32)
0xff80 0x0008 0x8c80 0x4a00 - road width above to left, same width as above (ie, xpos - 32)
0x0001						- road half/width above to left, (ie, xpos - 16)
0x0081						- road width to left as usual (xpos-16 from above, or 32 from above2)

0xfb0b 0x210b 0x8cf5 0x0dea - blue & left & right, with  blue|----|----|----|----|blue
in this mode changing columns 2 & 3 have no apparent effect
0xfb0b 0x7b09 0x8cf5 0x0dea - as above, but scaled up - perhaps 18 pixels shifted (twice that overall size)
0xfb0b 0x6c07 0x8cf5 0x0dea - as above, but scaled up - perhaps 40 pixels shifted from above
0xfb0b 0xaa06 0x8cf5 0x0dea - as above, but scaled up - perhaps 16 pixels shifted from above
0xfb0b 0xb005 0x8cf5 0x0dea - as above, but scaled up - perhaps 38 pixels shifted from above

b21	diff is 1a6
97b			20f
76c			c2
6aa			fa
5b0


0x0000 0x0008 0xxxxx 0xxxx - line starting at 0 for 128 pixels - 1 to 1 with road pixel data
0xff00 0x0008 0xxxxx 0xxxx - line starting at 32 for 128 pixels - 1 to 1 with road pixel data
0xfe00 0x0008 0xxxxx 0xxxx - line starting at 64 for 128 pixels - 1 to 1 with road pixel data



at standard zoom (0x800)
shift of 0x100 moves 32 pixels
so shift of 8 is assumed to move 1 pixel

at double zoom (0x1000)
assume shift of 0x100 only moves 16 pixels
so

0x100 * 0x400 => 0x40
0x100 * step 0x800 = must come out at 0x20
0x100 * step 0x1000 = must come out at 0x10
0x100 * step 0x2000 = 0x5

pos is 11.5 fixed point

-0x580 = middle
-0x180
-0x080
0
0x80

*/
	int y,x;
	int visible_line=0;
	const data16_t* data=roundup_r_ram;

	/* Road layer enable (?) */
	if ((roundup5_unknown0[0x2]&0x1)==0)
		return;

	/* Road data bank select (double buffered) */
	if (roundup5_e0000_ram[0]&0x10)
		data+=0x400;

	/* ??  Todo: This is wrong - don't know how to clip the road properly */
	y=256 - roundup5_unknown0[0xb];
	data+=y*4;

	visible_line=0;

	for (/*y=0*/; y<256; y++) {

		int shift=data[0];
		int shift2=data[2];
		int pal=4; /*(data[3]>>8)&0xf; */
		int step=((data[1]&0xff)<<8)|((data[1]&0xff00)>>8);
		int samplePos=0;
		const data16_t* linedata=roundup_p_ram;/* + (0x100 * pal); */
		int startPos=0, endPos=0;

		int palette_byte;/*=roundup_l_ram[visible_line/8]; */

		/*
			Each road line consists of up to two sets of 128 pixel data that can be positioned
			on the x-axis and stretched/compressed on the x-axis.  Any screen pixels to the left
			of the first set are drawn with pen 0 of the road pixel data.  Any screen pixels to the
			right of the second set line are drawn with pen 127 of the road pixel data.

			The road control data is laid out as follows (4 words per screen line, with 2 banks):

			Word 0:	Line shift for 1st set - 13.3 signed fixed point value.
			Word 1: Line scale - 5.11 fixed point value.  So 0x800 is 1:1, 0x400 is 1:2, etc
			Word 2: Line shift for 2nd set - 13.3 signed fixed point value.
			Word 3: ?

			The scale is shared between both pixel sets.  The 2nd set is only used when the road
			forks into two between stages.  The 2nd line shift is an offset from the last pixel
			of the 1st set.  The 2nd line shift uses a different palette bank.

2nd road uses upper palette - confirmed by water stage.
offset is from last pixel of first road segment?
last pixel of first road is really colour from 2nd road line? */


		palette_byte=roundup_l_ram[visible_line/8];
		pal=4 + ((palette_byte>>(visible_line%8))&1);

		visible_line++;

		if (shift&0x8000)
			shift=-(0x10000 - shift);
		shift=-shift;

		if (step)
			startPos=((shift<<8) + 0x80 )/ step;

		/* Fill in left of road segment */
		for (x=0; x<startPos && x<320; x++) {
			int col = linedata[0]&0xf;
			data8_t shadow=((data8_t*)shadow_bitmap->line[y])[x];
			if (shadow)
				plot_pixel(bitmap,x,y,Machine->pens[768 + pal*16 + col]);
			else
				plot_pixel(bitmap,x,y,Machine->pens[256 + pal*16 + col]);
		}

		/* If startpos is negative, clip it and adjust the sampling position accordingly */
		if (startPos<0) {
			samplePos=step*(0-startPos);
			startPos=0;
		} else {
			samplePos=0;
		}

		/* Fill in main part of road, then right-hand side edge */
		for (x=startPos; x<320 && (samplePos>>11)<0x80; x++) {
			/* look up colour */
			int col = linedata[(samplePos>>11)&0x7f]&0xf;
			data8_t shadow=((data8_t*)shadow_bitmap->line[y])[x];

			/* Clamp if we have reached the end of the pixel data */
			/*if ((samplePos>>11) > 0x7f) */
			/*	col=linedata[0x7f]&0xf; */

			if (shadow)
				plot_pixel(bitmap,x,y,Machine->pens[768 + pal*16 + col]);
			else
				plot_pixel(bitmap,x,y,Machine->pens[256 + pal*16 + col]);

			samplePos+=step;
		}

		/* Now work out how many pixels until start of 2nd segment */
		startPos=x;

		if (shift2&0x8000)
			shift2=-(0x10000 - shift2);
		shift2=-shift2;

		if (step)
			endPos=((shift2<<8) + 0x80) / step;
		else
			endPos=0;
		endPos-=128;
		endPos=startPos+endPos;

		/* Fill pixels */
		for (x=startPos; x<320 && x<endPos; x++) {
			int col = linedata[0x80]&0xf;
			data8_t shadow=((data8_t*)shadow_bitmap->line[y])[x];

			/* Clamp if we have reached the end of the pixel data */
			/*if ((samplePos>>11) > 0x7f) */
			/*	col=linedata[0x7f]&0xf; */

			if (shadow)
				plot_pixel(bitmap,x,y,Machine->pens[768 + pal*16 + col + 32]);
			else
				plot_pixel(bitmap,x,y,Machine->pens[256 + pal*16 + col + 32]);
		}

		if (endPos<0) {
			samplePos=step*(0-startPos);
		}
		else if (endPos<x) {
			samplePos=step*(x-endPos);
		} else {
			samplePos=0; /* todo */
		}

		for (/*x=endPos*/; x<320; x++) {
			/* look up colour */
			int col = linedata[((samplePos>>11)&0x7f) + 0x200]&0xf;
			data8_t shadow=((data8_t*)shadow_bitmap->line[y])[x];

			/* Clamp if we have reached the end of the pixel data */
			if ((samplePos>>11) > 0x7f)
				col=linedata[0x7f + 0x200]&0xf;

			if (shadow)
				plot_pixel(bitmap,x,y,Machine->pens[768 + pal*16 + 32 + col]);
			else
				plot_pixel(bitmap,x,y,Machine->pens[256 + pal*16 + 32 + col]);

			samplePos+=step;
		}
		data+=4;
	}
}

static void draw_bg(struct mame_bitmap *dst, const struct tilemap *src, const UINT16* scrollx, const UINT16* scrolly, const UINT16* tilemap_ram, int tile_bank, int xscroll_offset, int yscroll_offset, int xsize, int ysize)
{
	/*
		Each tile (0x4000 of them) has a lookup table in ROM to build an individual 3-bit palette
		from sets of 8 bit palettes!  
	*/
	const UINT8* tile_cluts = memory_region(REGION_GFX4);
	const struct mame_bitmap *src_bitmap = tilemap_get_pixmap(src);
	int src_y_mask=ysize-1;
	int src_x_mask=xsize-1;
	int tile_y_mask=(ysize/8)-1;
	int tile_x_mask=(xsize/8)-1;
	int tiles_per_x_line=(xsize/8);
	int x, y, p, pp, ppp;

	for (y=0; y<240; y++)
	{
		for (x=0; x<320; x++)
		{
			int src_x = x + scrollx[y] + xscroll_offset;
			int src_y = y + scrolly[y] + yscroll_offset;
			int tile_index = (((src_x>>3)&tile_x_mask) + (((src_y>>3)&tile_y_mask) * tiles_per_x_line));
			int bank = (tile_bank >> (((tilemap_ram[(tile_index+0x400)&0x7fff]&0xc00)>>10)*4))&0xf;
			int tile = (tilemap_ram[(tile_index+0x400)&0x7fff]&0x3ff) | (bank<<10);

			/*p=*BITMAP_ADDR16(src_bitmap, src_y&src_y_mask, src_x&src_x_mask); */
			p = ((UINT16 *)src_bitmap->line[src_y&src_y_mask])[src_x&src_x_mask]; /* correct i think */
			pp=tile_cluts[tile*8 + (p&0x7)];
			ppp=pp + ((p&0x78)<<5);

			if ((p&0x7)!=0 || ((p&0x7)==0 && (pp&0x7)!=0)) /* Not quite correct - some opaque pixels show as transparent */
			/*	*BITMAP_ADDR32(dst, y, x) = Machine->pens[ppp]; */
			/*plot_pixel(dst,y,x,Machine->pens[ppp]);*/ /* correct.?? */
            ((UINT32 *)dst->line[y])[x] = Machine->pens[ppp]; /* this seems correct */

		}
	}
}

static void update_cluts(int fake_palette_offset, int object_base, int length)
{
	/* Object palettes are build from a series of cluts stored in the object roms.

		We update 'Mame palettes' from the clut here in order to simplify the
		draw routines.  We also note down any uses of the 'shadow' pen (index 255).
	*/
	int i;
	UINT8 r,g,b;
	const data8_t* bank1=tatsumi_rom_clut0;
	const data8_t* bank2=tatsumi_rom_clut1;
	for (i=0; i<length; i+=8) {
		palette_get_color(bank1[1]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+0,r,g,b);
		shadow_pen_array[i+0]=(bank1[1]==255);
		palette_get_color(bank1[0]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+1,r,g,b);
		shadow_pen_array[i+1]=(bank1[0]==255);
		palette_get_color(bank1[3]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+2,r,g,b);
		shadow_pen_array[i+2]=(bank1[3]==255);
		palette_get_color(bank1[2]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+3,r,g,b);
		shadow_pen_array[i+3]=(bank1[2]==255);

		palette_get_color(bank2[1]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+4,r,g,b);
		shadow_pen_array[i+4]=(bank2[1]==255);
		palette_get_color(bank2[0]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+5,r,g,b);
		shadow_pen_array[i+5]=(bank2[0]==255);
		palette_get_color(bank2[3]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+6,r,g,b);
		shadow_pen_array[i+6]=(bank2[3]==255);
		palette_get_color(bank2[2]+object_base,&r,&g,&b);
		palette_set_color(fake_palette_offset+i+7,r,g,b);
		shadow_pen_array[i+7]=(bank2[2]==255);

		bank1+=4;
		bank2+=4;
	}
}

/**********************************************************************/

VIDEO_UPDATE( apache3 )
{
	update_cluts(1024, 0, 2048);

	fillbitmap(bitmap,Machine->pens[0],cliprect);
	draw_sky(bitmap, cliprect, 256, apache3_a0000[1]);
	draw_sprites(bitmap,cliprect,0, (tatsumi_sprite_control_ram[0x20]&0x1000) ? 0x1000 : 0);
	tilemap_draw(bitmap,cliprect,tx_layer,0,0);
}

VIDEO_UPDATE( roundup5 )
{
/*	data16_t bg_x_scroll=roundup5_unknown1[0] | (roundup5_unknown1[1]<<8); */
/*	data16_t bg_y_scroll=roundup5_unknown2[0] | (roundup5_unknown2[1]<<8); */

	update_cluts(1024, 512, 4096);

	tilemap_set_scrollx(tx_layer,0,24);
	tilemap_set_scrolly(tx_layer,0,0); /*(((roundupt_crt_reg[0xe]<<8)|roundupt_crt_reg[0xf])>>5) + 96); */

	fillbitmap(bitmap,Machine->pens[384],cliprect); /* todo */
	fillbitmap(priority_bitmap,0,cliprect);

	draw_sprites(priority_bitmap,cliprect,1,(tatsumi_sprite_control_ram[0xe0]&0x1000) ? 0x1000 : 0); /* Alpha pass only */
	draw_road(bitmap,cliprect,priority_bitmap);
	draw_sprites(bitmap,cliprect,0,(tatsumi_sprite_control_ram[0xe0]&0x1000) ? 0x1000 : 0); /* Full pass */
	tilemap_draw(bitmap,cliprect,tx_layer,0,0);
}

VIDEO_UPDATE( cyclwarr )
{
    bigfight_bank=bigfight_a40000[0];
	if (bigfight_bank!=bigfight_last_bank)
	{
		tilemap_mark_all_tiles_dirty(layer0);
		tilemap_mark_all_tiles_dirty(layer1);
		tilemap_mark_all_tiles_dirty(layer2);
		tilemap_mark_all_tiles_dirty(layer3);
		bigfight_last_bank=bigfight_bank;
	}

	fillbitmap(bitmap,Machine->pens[0],cliprect);

	draw_bg(bitmap, layer3, &cyclwarr_videoram1[0x000], &cyclwarr_videoram1[0x100], cyclwarr_videoram1, bigfight_a40000[0], 8, -0x80, 512, 4096);
	draw_bg(bitmap, layer2, &cyclwarr_videoram1[0x200], &cyclwarr_videoram1[0x300], cyclwarr_videoram1, bigfight_a40000[0], 8, -0x80, 512, 4096);
	draw_bg(bitmap, layer1, &cyclwarr_videoram0[0x000], &cyclwarr_videoram0[0x100], cyclwarr_videoram0, bigfight_a40000[0], 8, -0x40, 1024, 2048);
	update_cluts(8192, 4096, 8192);
	draw_sprites(bitmap,cliprect,0,(tatsumi_sprite_control_ram[0xe0]&0x1000) ? 0x1000 : 0);
	draw_bg(bitmap, layer0, &cyclwarr_videoram0[0x200], &cyclwarr_videoram0[0x300], cyclwarr_videoram0, bigfight_a40000[0], 0x10, -0x80, 512, 4096); 

}

VIDEO_UPDATE( bigfight )
{
	bigfight_bank=bigfight_a40000[0];
	if (bigfight_bank!=bigfight_last_bank)
	{
		tilemap_mark_all_tiles_dirty(layer0);
		tilemap_mark_all_tiles_dirty(layer1);
		tilemap_mark_all_tiles_dirty(layer2);
		tilemap_mark_all_tiles_dirty(layer3);
		bigfight_last_bank=bigfight_bank;
	}

	fillbitmap(bitmap,Machine->pens[0],cliprect);
	draw_bg(bitmap, layer3, &cyclwarr_videoram1[0x000], &cyclwarr_videoram1[0x100], cyclwarr_videoram1, bigfight_a40000[0], 8, -0x40, 1024, 2048);
	draw_bg(bitmap, layer2, &cyclwarr_videoram1[0x200], &cyclwarr_videoram1[0x300], cyclwarr_videoram1, bigfight_a40000[0], 8, -0x40, 1024, 2048);
	draw_bg(bitmap, layer1, &cyclwarr_videoram0[0x000], &cyclwarr_videoram0[0x100], cyclwarr_videoram0, bigfight_a40000[0], 8, -0x40, 1024, 2048);
	update_cluts(8192, 4096, 8192);
	draw_sprites(bitmap,cliprect,0,(tatsumi_sprite_control_ram[0xe0]&0x1000) ? 0x1000 : 0);
	draw_bg(bitmap, layer0, &cyclwarr_videoram0[0x200], &cyclwarr_videoram0[0x300], cyclwarr_videoram0, bigfight_a40000[0], 0x10, -0x40, 1024, 2048);

}
