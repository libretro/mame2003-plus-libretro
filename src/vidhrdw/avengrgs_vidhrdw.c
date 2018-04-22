#include "driver.h"
#include "vidhrdw/generic.h"

data32_t *avengrgs_vram, *avengrgs_ram1, *avengrgs_ram2;

/******************************************************************************/

VIDEO_START( avengrgs )
{
	return 0;
}

static void draw_sprites(struct mame_bitmap *bitmap,const struct rectangle *cliprect)
{
	data32_t *index_ptr;
	int offs,fx=0,fy=0,x,y,color,sprite,indx,h,w,bank,bx,by;
	int xmult,ymult,xoffs,yoffs;
	data8_t *rom = memory_region(REGION_GFX4) + 0x20000, *index_ptr8;

/*	for (offs = 0; offs<0x3000/4; offs+=8)*/
	for (offs = (0x3000/4)-8; offs>=0; offs-=8)
	{
		if ((spriteram32[offs+0]&0x8000)==0)
			continue; /*check*/

		y = spriteram32[offs+2]&0x7ff;
		x = spriteram32[offs+3]&0x7ff; /* Bit 0100 0000 sometimes set?? sh2 bug? */

		if (x&0x400) x=-(0x400-(x&0x3ff));
		if (y&0x400) y=-(0x400-(y&0x3ff));

		fx = spriteram32[offs+1]&0x8000;
		fy = spriteram32[offs+1]&0x4000;
		color = spriteram32[offs+1]&0x7f;
		indx = spriteram32[offs+0]&0x3fff;

		/* Lookup tiles/size in sprite index ram OR in the lookup rom */
		if (spriteram32[offs+0]&0x4000) {
			index_ptr8=rom + indx*8; /* Byte ptr */
			h=(index_ptr8[1]>>0)&0xf;
			w=(index_ptr8[3]>>0)&0xf;
			sprite = (index_ptr8[7]<<8)|index_ptr8[6];
			bank = index_ptr8[4]&3;
			/*unused byte 5*/
			yoffs=index_ptr8[0]&0xff;
			xoffs=index_ptr8[2]&0xff;
			if (index_ptr8[4]&0x40)  {
				sprite&=0x1fff; /*TODO - wrong*/
				bank=0;
			}
		} else {
			indx&=0x1fff;
			index_ptr=avengrgs_ram1 + indx*4;
			h=(index_ptr[0]>>8)&0xf;
			w=(index_ptr[1]>>8)&0xf;
			sprite = index_ptr[3]&0xffff;
			bank = index_ptr[2]&3;
			if (index_ptr[2]&0x40) {
				sprite&=0x1fff;  /*TODO - wrong*/
				bank=0;
			}
/*	color=rand()%0x7f;*/
			yoffs=index_ptr[0]&0xff;
			xoffs=index_ptr[1]&0xff;
		}
		if (bank==3) color=rand()%0x7f;
		if (bank==3) bank=1; /* Mirror, no roms for bank 3 */

/*if (xoffs&0x80)*/
/*xoffs=- (0x80 - (xoffs&0x7f));*/

		if (fx) x+=xoffs; else x-=xoffs; /*check for signed offsets...*/
		if (fy) y+=yoffs-16; else y-=yoffs; /*check for signed offsets...*/

		color|=0x80;

		if (fx) xmult=-16; else xmult=16;
		if (fy) ymult=-16; else ymult=16;

		for (by=0; by<h; by++) {
			for (bx=0; bx<w; bx++) {
				drawgfx(bitmap,Machine->gfx[bank],
						sprite,
						color,fx,fy,x+(bx*xmult),y+(by*ymult),
						cliprect,TRANSPARENCY_PEN,0);
				sprite++;
			}
		}
	}
}

/*

	0100 0000 bug...

  4964 - calls function
	6a6c - which sets 3c@SP

	6bf8 - uses 3c@SP

	SHLR8 at 6a4e creates 0100 0000

*/

VIDEO_UPDATE( avengrgs )
{
	int mx,my;
	data32_t *vram_ptr=avengrgs_ram1 + (0x1dc00/4);

#if 0
/*	data8_t *rom = memory_region(REGION_GFX4);*/

/*	static int bank=0;*/
/*	static int base=0x40000;*/
/*	int o=0;*/

/*	if (keyboard_pressed_memory(KEYCODE_X))*/
/*		base+=0x200;*/
/*	if (keyboard_pressed_memory(KEYCODE_Z))*/
/*		base-=0x200;*/

/* 22a65c0 == linescroll*/

	usrintf_showmessage("%08x",base);

	for (my=0; my<16; my++) {
		for (mx=0; mx<16; mx++) {
			int t=rom[base+o]|(rom[base+1+o]<<8);
			drawgfx(bitmap,Machine->gfx[bank],t,5,0,0,mx*16,my*16,0,TRANSPARENCY_PEN,0);
			o+=2;
		}
	}
#endif

	fillbitmap(bitmap,get_black_pen(),cliprect);

	for (my=0; my<32; my++) {
		for (mx=0; mx<64; mx++) {
			int indx=0;

			if (mx<16)
				indx=mx + (my&0xf)*0x20;
			else if (mx<32)
				indx=(mx-16) + (my&0xf)*0x20 + 0x200;
			else
				indx=(mx-32) + (my&0xf)*0x20 + 0x400;

			if (my>15)
				indx+=0x10;

			if ((vram_ptr[indx])&0xff)
				drawgfx(bitmap,Machine->gfx[3],(vram_ptr[indx])&0xff,15-((vram_ptr[indx]>>12)&0xf),0,0,mx*8,my*8,0,TRANSPARENCY_PEN,0);
		}
	}

	draw_sprites(bitmap,cliprect);
}

VIDEO_STOP(avengrgs)
{
#if 0
	FILE *fp;
	int i;
	char a[4];

	fp=fopen("vram.dmp","wb");
	for (i=0; i<0x20000/4; i++) {
		a[0]=avengrgs_ram1[i]&0xff;
		a[1]=avengrgs_ram1[i]>>8;
		fwrite(a,0x2,1,fp);
	}
	fclose(fp);

	fp=fopen("vram2.dmp","wb");
	fwrite(avengrgs_ram2,0x4000,1,fp);
	fclose(fp);
#endif
}
