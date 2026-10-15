/*
   Raiden II driver - Salvia/MAME2003+ playability backport

   This is a targeted backport of the later working Raiden II emulation to
   the MAME 0.78-style API used by Salvia's mame2003-plus core.

   Key additions over the old preliminary driver:
   - V30 program banking
   - Raiden II COP/protection command simulation
   - sprite protection/DMA/sort helpers
   - correct tile banks, CRTC scroll/layer state
   - correct 4-word sprite format + priority passes
   - host-independent Raiden II sprite ROM interleave/decryption
   - Seibu YM2151 + dual OKIM6295 sound path
   - correct input/system/coin layout and sane DIP defaults

   Source basis: later MAME Raiden II driver/r2crypt and current FBNeo's
   Raiden II COP implementation, adapted to this old core API.
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/seibu.h"
#include "sound/2151intf.h"
#include "sound/adpcm.h"
#include "state.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* seibu_pending_w exists in sndhrdw/seibu.c but is not declared by the old header */
WRITE_HANDLER( seibu_pending_w );

static struct tilemap *background_layer,*midground_layer,*foreground_layer,*text_layer;
static unsigned char *back_data,*fore_data,*mid_data;
static int bg_bank, mid_bank, fg_bank, tx_bank;
static UINT8 r2_scroll[12];
static UINT16 r2_layer_enable;
static UINT16 r2_prg_bank;
/* The old memory system uses REGION_CPU1[0..1ffff] as writable RAM.
   Preserve both switchable ROM pages before the first CPU/reset runs. */
static UINT8 *r2_bank_rom;

//#include "raiden2_debug.h"

//#include "raiden2_r2crypt.inc"
//#include "raiden2_cop.inc"
//#include "raiden2_debug.inc"
//#include "raiden2_sound.inc"

static unsigned r2_video_enable(void)
{
//#if RAIDEN2_DEBUG && RAIDEN2_FORCE_VIDEO_DIAG
// return 0x1f ^ (1U << (RAIDEN2_FORCE_VIDEO_DIAG - 1));
//#else
 return r2_layer_enable;
//#endif
}

/*
 * Raiden II specific Seibu COP simulation backported from the later working
 * MAME/FBNeo implementations.  This is intentionally V30 little-endian and
 * does not reuse the old 68k-oriented generic seicop memory helpers.
 */

#define R2_COP_BASE 0x00400
#define R2_COP_END  0x006ff

static UINT8 r2_copram[0x300];
static UINT32 r2_cop_regs[8], r2_cop_itoa;
static UINT16 r2_cop_status, r2_cop_scale, r2_cop_itoa_digit_count;
static UINT16 r2_cop_angle, r2_cop_dist, r2_cop_angle_target, r2_cop_angle_step;
static UINT8 r2_cop_itoa_digits[10];
static UINT16 r2_cop_dma_mode;
static UINT16 r2_cop_dma_src[0x200], r2_cop_dma_dst[0x200], r2_cop_dma_size[0x200];
static UINT16 r2_cop_dma_v1, r2_cop_dma_v2, r2_cop_dma_adr_rel;
static UINT16 r2_sprites_cur_start;
static UINT16 r2_pal_brightness_val;
static UINT16 r2_cop_bank;
static UINT16 r2_sprite_prot_x, r2_sprite_prot_y, r2_dst1, r2_cop_spr_maxx, r2_cop_spr_off;
static UINT16 r2_sprite_prot_src_addr[2];
static UINT16 r2_cop_hit_status, r2_cop_hit_baseadr, r2_cop_hit_val_stat;
static INT16 r2_cop_hit_val[3];
static UINT32 r2_cop_sort_ram_addr, r2_cop_sort_lookup;
static UINT16 r2_cop_sort_param;

struct r2_collision_info
{
	INT16 pos[3];
	INT8 dx[3];
	UINT8 size[3];
	int allow_swap;
	UINT16 flags_swap;
	UINT32 spradr;
	INT16 min[3], max[3];
};
static struct r2_collision_info r2_collision[2];

static UINT8 r2_mem_r8(UINT32 a)
{
	return (UINT8)cpu_readmem20(a & 0xfffff);
}

static UINT16 r2_mem_r16(UINT32 a)
{
	return (UINT16)(r2_mem_r8(a) | ((UINT16)r2_mem_r8(a + 1) << 8));
}

static UINT32 r2_mem_r32(UINT32 a)
{
	return (UINT32)r2_mem_r16(a) | ((UINT32)r2_mem_r16(a + 2) << 16);
}

static void r2_mem_w8(UINT32 a, UINT8 v)
{
	cpu_writemem20(a & 0xfffff, v);
}

static void r2_mem_w16(UINT32 a, UINT16 v)
{
	r2_mem_w8(a, (UINT8)v);
	r2_mem_w8(a + 1, (UINT8)(v >> 8));
}

static void r2_mem_w32(UINT32 a, UINT32 v)
{
	r2_mem_w16(a, (UINT16)v);
	r2_mem_w16(a + 2, (UINT16)(v >> 16));
}

static UINT16 r2_copram_word(UINT32 addr)
{
	UINT32 o = (addr & 0x7fe) - R2_COP_BASE;
	if (o + 1 >= sizeof(r2_copram)) return 0xffff;
	return (UINT16)(r2_copram[o] | ((UINT16)r2_copram[o + 1] << 8));
}

static void r2_reset_cop(void)
{
	memset(r2_copram, 0, sizeof(r2_copram));
	memset(r2_cop_regs, 0, sizeof(r2_cop_regs));
	r2_cop_itoa = 0;
	r2_cop_status = 0;
	r2_cop_scale = 0;
	r2_cop_itoa_digit_count = 4;
	r2_cop_angle = r2_cop_dist = 0;
	r2_cop_angle_target = r2_cop_angle_step = 0;
	memset(r2_cop_itoa_digits, 0, sizeof(r2_cop_itoa_digits));
	r2_cop_dma_mode = 0;
	memset(r2_cop_dma_src, 0, sizeof(r2_cop_dma_src));
	memset(r2_cop_dma_dst, 0, sizeof(r2_cop_dma_dst));
	memset(r2_cop_dma_size, 0, sizeof(r2_cop_dma_size));
	r2_cop_dma_v1 = r2_cop_dma_v2 = r2_cop_dma_adr_rel = 0;
	r2_sprites_cur_start = 0;
	r2_pal_brightness_val = 0;
	r2_cop_bank = 0;
	r2_sprite_prot_x = r2_sprite_prot_y = r2_dst1 = r2_cop_spr_maxx = r2_cop_spr_off = 0;
	memset(r2_sprite_prot_src_addr, 0, sizeof(r2_sprite_prot_src_addr));
	memset(r2_collision, 0, sizeof(r2_collision));
	r2_cop_hit_status = r2_cop_hit_baseadr = r2_cop_hit_val_stat = 0;
	memset(r2_cop_hit_val, 0, sizeof(r2_cop_hit_val));
	r2_cop_sort_ram_addr = r2_cop_sort_lookup = 0;
	r2_cop_sort_param = 0;
}

static void r2_itoa_compute(void)
{
	int digits = r2_cop_itoa_digit_count >= 2 ? 9 : 1 << (r2_cop_itoa_digit_count * 2);
	UINT32 val = r2_cop_itoa;
	int i;
	if (digits > 9) digits = 9;
	for (i = 0; i < digits; ++i)
	{
		if (!val && i) r2_cop_itoa_digits[i] = 0x20;
		else
		{
			r2_cop_itoa_digits[i] = (UINT8)(0x30 | (val % 10));
			val /= 10;
		}
	}
	r2_cop_itoa_digits[9] = 0;
}

static void r2_sprite_prot_src_write(UINT16 data)
{
	UINT32 src;
	INT32 x, y, w, h;
	UINT16 head1, head2, flag;
	r2_sprite_prot_src_addr[1] = data;
	src = ((UINT32)r2_sprite_prot_src_addr[0] << 4) + r2_sprite_prot_src_addr[1];
	x = (INT16)((INT32)(r2_mem_r32(src + 0x08) >> 16) - (INT32)r2_sprite_prot_x);
	y = (INT16)((INT32)(r2_mem_r32(src + 0x04) >> 16) - (INT32)r2_sprite_prot_y);
	head1 = r2_mem_r16(src + r2_cop_spr_off);
	head2 = r2_mem_r16(src + r2_cop_spr_off + 2);
	w = ((((head1 >> 8) & 7) + 1) << 4);
	h = ((((head1 >> 12) & 7) + 1) << 4);
	flag = (x - w / 2 > -w && x - w / 2 < (INT32)r2_cop_spr_maxx + w &&
	        y - h / 2 > -h && y - h / 2 < 256 + h) ? 1 : 0;
	flag = (r2_mem_r16(src) & 0xfffe) | flag;
	r2_mem_w16(src, flag);
	if (flag & 1)
	{
		r2_mem_w16(r2_dst1, head1);
		r2_mem_w16(r2_dst1 + 2, head2);
		r2_mem_w16(r2_dst1 + 4, (UINT16)(x - w / 2));
		r2_mem_w16(r2_dst1 + 6, (UINT16)(y - h / 2));
		r2_dst1 += 8;
	}
}

static void r2_collision_read_pos(int slot, UINT32 spradr, int allow_swap)
{
	int i;
	r2_collision[slot].allow_swap = allow_swap;
	r2_collision[slot].flags_swap = r2_mem_r16(spradr + 2);
	r2_collision[slot].spradr = spradr;
	for (i = 0; i < 3; ++i) r2_collision[slot].pos[i] = (INT16)r2_mem_r16(spradr + 6 + 4 * i);
}

static void r2_collision_update_hitbox(int slot, UINT32 hitadr, UINT16 data)
{
	UINT32 hitadr2 = r2_mem_r16(hitadr) | ((UINT32)r2_cop_hit_baseadr << 16);
	int num_axis = (data & 0x0100) ? 3 : 2;
	INT16 dx[3], size[3];
	UINT8 res = (num_axis == 3) ? 7 : 3;
	int i;

	for (i = 0; i < 3; ++i)
	{
		r2_collision[slot].dx[i] = 0;
		r2_collision[slot].size[i] = 0;
		dx[i] = size[i] = 0;
	}
	for (i = 0; i < num_axis; ++i)
	{
		r2_collision[slot].dx[i] = (INT8)r2_mem_r8(hitadr2++);
		r2_collision[slot].size[i] = r2_mem_r8(hitadr2++);
		dx[i] = r2_collision[slot].dx[i];
		size[i] = r2_collision[slot].size[i];
	}

	for (i = 0; i < num_axis; ++i)
	{
		if (r2_collision[slot].allow_swap && (r2_collision[slot].flags_swap & (1 << i)))
		{
			r2_collision[slot].max[i] = r2_collision[slot].pos[i] - dx[i];
			r2_collision[slot].min[i] = r2_collision[slot].max[i] - size[i];
		}
		else
		{
			r2_collision[slot].min[i] = r2_collision[slot].pos[i] + dx[i];
			r2_collision[slot].max[i] = r2_collision[slot].min[i] + size[i];
		}
		if (r2_collision[0].max[i] > r2_collision[1].min[i] && r2_collision[0].min[i] < r2_collision[1].max[i]) res &= ~(1 << i);
		if (r2_collision[1].max[i] > r2_collision[0].min[i] && r2_collision[1].min[i] < r2_collision[0].max[i]) res &= ~(1 << i);
		r2_cop_hit_val[i] = r2_collision[0].pos[i] - r2_collision[1].pos[i];
	}
	r2_cop_hit_val_stat = res;
	r2_cop_hit_status = res;
}

static void r2_cop_command(int offset, UINT16 data)
{
	INT32 dx, dy, delta, div, raw_angle, amp, res;
	UINT32 ppos, npos, v1, v2, v3;
	double angle;
	offset /= 2;
	r2_cop_status &= 0x7fff;

	switch (data)
	{
		case 0x0205:
			ppos = r2_mem_r32(r2_cop_regs[0] + 4 + offset * 4);
			npos = ppos + r2_mem_r32(r2_cop_regs[0] + 0x10 + offset * 4);
			delta = (INT32)(INT16)(npos >> 16) - (INT32)(INT16)(ppos >> 16);
			r2_mem_w32(r2_cop_regs[0] + 4 + offset * 4, npos);
			r2_mem_w16(r2_cop_regs[0] + 0x1e + offset * 4,
			             (UINT16)(r2_mem_r16(r2_cop_regs[0] + 0x1e + offset * 4) + delta));
			break;
		case 0x0904:
			r2_mem_w32(r2_cop_regs[0] + 16 + offset * 4,
			             r2_mem_r32(r2_cop_regs[0] + 16 + offset * 4) - r2_mem_r32(r2_cop_regs[0] + 0x28 + offset * 4));
			break;
		case 0x0905:
			r2_mem_w32(r2_cop_regs[0] + 16 + offset * 4,
			             r2_mem_r32(r2_cop_regs[0] + 16 + offset * 4) + r2_mem_r32(r2_cop_regs[0] + 0x28 + offset * 4));
			break;
		case 0x130e: case 0x138e: case 0x330e: case 0x338e:
			dx = (INT32)r2_mem_r32(r2_cop_regs[1] + 4) - (INT32)r2_mem_r32(r2_cop_regs[0] + 4);
			dy = (INT32)r2_mem_r32(r2_cop_regs[1] + 8) - (INT32)r2_mem_r32(r2_cop_regs[0] + 8);
			if (!dy) { r2_cop_status |= 0x8000; r2_cop_angle = 0; }
			else { r2_cop_angle = (UINT16)((INT32)(atan((double)dx / (double)dy) * 128.0 / M_PI)); if (dy < 0) r2_cop_angle += 0x80; }
			if (data & 0x0080) r2_mem_w8(r2_cop_regs[0] + 0x34, (UINT8)r2_cop_angle);
			break;
		case 0x2208: case 0x2288:
			dx = (INT16)r2_mem_r16(r2_cop_regs[0] + 0x12);
			dy = (INT16)r2_mem_r16(r2_cop_regs[0] + 0x16);
			if (!dy) { r2_cop_status |= 0x8000; r2_cop_angle = 0; }
			else { r2_cop_angle = (UINT16)((INT32)(atan((double)dx / (double)dy) * 128.0 / M_PI)); if (dy < 0) r2_cop_angle += 0x80; }
			if (data & 0x0080) r2_mem_w8(r2_cop_regs[0] + 0x34, (UINT8)r2_cop_angle);
			break;
		case 0x2a05:
			delta = (INT16)r2_mem_r16(r2_cop_regs[1] + 0x1e + offset * 4);
			r2_mem_w16(r2_cop_regs[0] + 6 + offset * 4, (UINT16)(r2_mem_r16(r2_cop_regs[0] + 6 + offset * 4) + delta));
			r2_mem_w16(r2_cop_regs[0] + 0x1e + offset * 4, (UINT16)(r2_mem_r16(r2_cop_regs[0] + 0x1e + offset * 4) + delta));
			break;
		case 0x39b0: case 0x3b30: case 0x3bb0:
			dx = ((INT32)r2_mem_r32(r2_cop_regs[1] + 4) - (INT32)r2_mem_r32(r2_cop_regs[0] + 4)) >> 16;
			dy = ((INT32)r2_mem_r32(r2_cop_regs[1] + 8) - (INT32)r2_mem_r32(r2_cop_regs[0] + 8)) >> 16;
			r2_cop_dist = (UINT16)sqrt((double)dx * dx + (double)dy * dy);
			if (data & 0x0080) r2_mem_w16(r2_cop_regs[0] + ((data & 0x0200) ? 0x3a : 0x38), r2_cop_dist);
			break;
		case 0x42c2:
			div = r2_mem_r16(r2_cop_regs[0] + 0x36); if (!div) div = 1;
			r2_cop_status = 7;
			r2_mem_w16(r2_cop_regs[0] + 0x38, (UINT16)((r2_cop_dist << (5 - r2_cop_scale)) / div));
			break;
		case 0x4aa0:
			div = r2_mem_r16(r2_cop_regs[0] + 0x38); if (!div) div = 1;
			r2_cop_status = 7;
			r2_mem_w16(r2_cop_regs[0] + 0x36, (UINT16)((r2_cop_dist << (5 - r2_cop_scale)) / div));
			break;
		case 0x6200:
			raw_angle = r2_mem_r8(r2_cop_regs[0] + 0x34);
			r2_cop_angle_target &= 0xff; r2_cop_angle_step &= 0xff;
			v1 = r2_mem_r16(r2_cop_regs[0]) & ~0x0004U;
			delta = raw_angle - r2_cop_angle_target;
			if (delta >= 128) delta -= 256; else if (delta < -128) delta += 256;
			if (delta < 0) { if (delta >= -(INT32)r2_cop_angle_step) { raw_angle = r2_cop_angle_target; v1 |= 4; } else raw_angle += r2_cop_angle_step; }
			else { if (delta <= (INT32)r2_cop_angle_step) { raw_angle = r2_cop_angle_target; v1 |= 4; } else raw_angle -= r2_cop_angle_step; }
			r2_mem_w16(r2_cop_regs[0], (UINT16)v1); r2_mem_w8(r2_cop_regs[0] + 0x34, (UINT8)raw_angle);
			break;
		case 0x8100:
			raw_angle = r2_mem_r16(r2_cop_regs[0] + 0x34) & 0xff;
			angle = raw_angle * M_PI / 128.0;
			amp = (65536 >> 5) * (r2_mem_r16(r2_cop_regs[0] + 0x36) & 0xff);
			if (raw_angle == 0xc0) amp *= 2;
			res = ((INT32)(amp * sin(angle))) << r2_cop_scale;
			r2_mem_w32(r2_cop_regs[0] + 16, (UINT32)res);
			break;
		case 0x8900:
			raw_angle = r2_mem_r16(r2_cop_regs[0] + 0x34) & 0xff;
			angle = raw_angle * M_PI / 128.0;
			amp = (65536 >> 5) * (r2_mem_r16(r2_cop_regs[0] + 0x36) & 0xff);
			if (raw_angle == 0x80) amp *= 2;
			res = ((INT32)(amp * cos(angle))) << r2_cop_scale;
			r2_mem_w32(r2_cop_regs[0] + 20, (UINT32)res);
			break;
		case 0x5205: case 0x5a05:
			r2_mem_w32(r2_cop_regs[1], r2_mem_r32(r2_cop_regs[0]));
			break;
		case 0xf205:
			r2_mem_w32(r2_cop_regs[2], r2_mem_r32(r2_cop_regs[0] + 4));
			break;
		case 0xa100: case 0xa180:
			r2_collision_read_pos(0, r2_cop_regs[0], data & 0x0080); break;
		case 0xa900: case 0xa980:
			r2_collision_read_pos(1, r2_cop_regs[1], data & 0x0080); break;
		case 0xb100:
			r2_collision_update_hitbox(0, r2_cop_regs[2], data); break;
		case 0xb900:
			r2_collision_update_hitbox(1, r2_cop_regs[3], data); break;
		case 0xede5:
			v1 = r2_mem_r32(r2_cop_regs[4] + 8); v2 = r2_mem_r32(r2_cop_regs[4] + 12); v3 = r2_mem_r32(r2_cop_regs[4] + 16);
			if (v3) r2_mem_w16(r2_cop_regs[4] + 22, (UINT16)((v1 + v2) / v3));
			break;
		case 0xf790:
			r2_cop_dist = (UINT16)sqrt((double)r2_mem_r32(r2_cop_regs[4] + 8)); break;
		case 0xfc84:
			r2_mem_w32(r2_cop_regs[4] + 8, r2_mem_r32(r2_cop_regs[4]) - r2_mem_r32(r2_cop_regs[4] + 4)); break;
		default:
			break;
	}
}

static UINT8 r2_fade_table(UINT16 v)
{
	int low = v & 0x1f;
	int high = v & 0x3e0;
	return (UINT8)((low * (high | (high >> 5)) + 0x210) >> 10);
}

static void r2_cop_dma_trigger(void)
{
	UINT32 src, dst, size, i, address, length;
	UINT16 pal_val, w;
	int r, g, b, rt, gt, bt;
	int rsize;
	UINT16 mode = r2_cop_dma_mode & 0x1ff;

	switch (mode)
	{
		case 0x14:
			rsize = ((0x80 - r2_cop_dma_size[mode]) & 0x7f) + 1;
			r2_sprites_cur_start = (UINT16)(0x1000 - (rsize << 5));
			break;
		case 0x82:
			src = (UINT32)r2_cop_dma_src[mode] << 6;
			dst = (UINT32)r2_cop_dma_dst[mode] << 6;
			size = (((UINT32)r2_cop_dma_size[mode] << 5) - ((UINT32)r2_cop_dma_dst[mode] << 6) + 0x20) / 2;
			for (i = 0; i < size; ++i)
			{
				w = r2_mem_r16(src + (r2_cop_dma_adr_rel * 0x400));
				bt = r2_fade_table(((w & 0x7c00) >> 5) | (r2_pal_brightness_val ^ 0));
				w = r2_mem_r16(src);
				b = r2_fade_table(((w & 0x7c00) >> 5) | (r2_pal_brightness_val ^ 0x1f));
				pal_val = (UINT16)(((b + bt) & 0x1f) << 10);
				w = r2_mem_r16(src + (r2_cop_dma_adr_rel * 0x400));
				gt = r2_fade_table((w & 0x03e0) | (r2_pal_brightness_val ^ 0));
				w = r2_mem_r16(src);
				g = r2_fade_table((w & 0x03e0) | (r2_pal_brightness_val ^ 0x1f));
				pal_val |= (UINT16)(((g + gt) & 0x1f) << 5);
				w = r2_mem_r16(src + (r2_cop_dma_adr_rel * 0x400));
				rt = r2_fade_table(((w & 0x001f) << 5) | (r2_pal_brightness_val ^ 0));
				w = r2_mem_r16(src);
				r = r2_fade_table(((w & 0x001f) << 5) | (r2_pal_brightness_val ^ 0x1f));
				pal_val |= (UINT16)((r + rt) & 0x1f);
				r2_mem_w16(dst, pal_val);
				src += 2; dst += 2;
			}
			break;
		case 0x09:
			src = (UINT32)r2_cop_dma_src[mode] << 6;
			dst = (UINT32)r2_cop_dma_dst[mode] << 6;
			size = (((UINT32)r2_cop_dma_size[mode] << 5) - ((UINT32)r2_cop_dma_dst[mode] << 6) + 0x20) / 2;
			for (i = 0; i < size; ++i) { r2_mem_w16(dst, r2_mem_r16(src)); src += 2; dst += 2; }
			break;
		case 0x118: case 0x11f:
			if (r2_cop_dma_dst[mode] != 0) break;
			address = (UINT32)r2_cop_dma_src[mode] << 6;
			length = ((UINT32)r2_cop_dma_size[mode] + 1) << 5;
			for (i = address; i < address + length; i += 4)
				r2_mem_w32(i, (UINT32)r2_cop_dma_v1 | ((UINT32)r2_cop_dma_v2 << 16));
			break;
		default:
			break;
	}
}

static void r2_cop_sort_trigger(UINT16 count)
{
	UINT32 i, j;
	UINT16 a, b;
	UINT32 ka, kb;
	if (r2_cop_sort_param != 1 && r2_cop_sort_param != 2) return;
	/* Stable insertion: equal depth entries must retain upload order,
	   as in FBNeo's stable merge. No STL, allocation or host-endian casts. */
	for (i = 1; i < count; ++i)
	{
		a = r2_mem_r16(r2_cop_sort_lookup + i * 2);
		ka = r2_mem_r32(r2_cop_sort_ram_addr + a);
		for (j = i; j > 0; --j)
		{
			b = r2_mem_r16(r2_cop_sort_lookup + (j - 1) * 2);
			kb = r2_mem_r32(r2_cop_sort_ram_addr + b);
			if (r2_cop_sort_param == 1 ? kb <= ka : kb >= ka) break;
			r2_mem_w16(r2_cop_sort_lookup + j * 2, b);
		}
		r2_mem_w16(r2_cop_sort_lookup + j * 2, a);
	}
}

static void r2_crtc_write(UINT32 addr, UINT8 data)
{
	UINT32 o = addr & 0x7f;
	if ((o & 0x7e) == 0x1c)
	{
		if (o & 1) r2_layer_enable = (r2_layer_enable & 0x00ff) | ((UINT16)data << 8);
		else r2_layer_enable = (r2_layer_enable & 0xff00) | data;
	}
	if (o >= 0x20 && o <= 0x2b) r2_scroll[o & 0x0f] = data;
}

static void r2_main_bankswitch(UINT16 data)
{
	UINT8 *rom = r2_bank_rom;
	int bb = (~(data >> 15)) & 1;
	r2_prg_bank = data;
	cpu_setbank(3, rom + bb * 0x20000);
	cpu_setbank(4, rom + bb * 0x20000 + 0x10000);
//	if (cpu_getactivecpu() == 0) r2_debug_bank();
}

static READ_HANDLER(r2_cop_r)
{
	UINT32 addr = R2_COP_BASE + offset;
	UINT16 ret;
	UINT32 pair = addr & 0x7fe;
	ret = r2_copram_word(pair);
	switch (pair)
	{
		case 0x580: ret = r2_cop_hit_status; break;
		case 0x582: case 0x584: case 0x586: ret = (UINT16)r2_cop_hit_val[(pair - 0x582) / 2]; break;
		case 0x588: ret = r2_cop_hit_val_stat; break;
		case 0x590: case 0x592: case 0x594: case 0x596: case 0x598:
		{
			int d = pair & 0x0e;
			ret = r2_cop_itoa_digits[d] | ((UINT16)r2_cop_itoa_digits[d + 1] << 8);
			break;
		}
		case 0x5b0: ret = r2_cop_status; break;
		case 0x5b2: ret = r2_cop_dist; break;
		case 0x5b4: ret = r2_cop_angle; break;
		case 0x6c0: ret = r2_cop_spr_off; break;
		case 0x6c2: ret = r2_sprite_prot_src_addr[0]; break;
		case 0x6dc: ret = r2_cop_spr_maxx; break;
		default: break;
	}
//	r2_debug_access(addr, ret & (0xff << ((addr & 1)*8)), 0);
	return (ret >> ((addr & 1) * 8)) & 0xff;
}

static WRITE_HANDLER(r2_cop_w)
{
	UINT32 addr = R2_COP_BASE + offset;
	UINT32 o = addr - R2_COP_BASE;
	UINT32 pair = addr & 0x7fe;
	UINT16 w;
	int idx;
	if (o < sizeof(r2_copram)) r2_copram[o] = data;
//	r2_debug_access(addr, r2_copram_word(pair), 1);
	if (addr >= 0x600 && addr <= 0x64f) r2_crtc_write(addr, (UINT8)data);
	if (!(addr & 1)) return;
	w = r2_copram_word(pair);

	switch (pair)
	{
		case 0x41c: r2_cop_angle_target = w; break;
		case 0x41e: r2_cop_angle_step = w; break;
		case 0x420: r2_cop_itoa = (r2_cop_itoa & 0xffff0000U) | w; r2_itoa_compute(); break;
		case 0x422: r2_cop_itoa = (r2_cop_itoa & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x424: r2_cop_itoa_digit_count = w; break;
		case 0x428: r2_cop_dma_v1 = w; break;
		case 0x42a: r2_cop_dma_v2 = w; break;
		case 0x436: r2_cop_hit_baseadr = w; break;
		case 0x444: r2_cop_scale = w & 3; break;
		case 0x450: r2_cop_sort_ram_addr = (r2_cop_sort_ram_addr & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x452: r2_cop_sort_ram_addr = (r2_cop_sort_ram_addr & 0xffff0000U) | w; break;
		case 0x454: r2_cop_sort_lookup = (r2_cop_sort_lookup & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x456: r2_cop_sort_lookup = (r2_cop_sort_lookup & 0xffff0000U) | w; break;
		case 0x458: r2_cop_sort_param = w; break;
		case 0x45a: r2_pal_brightness_val = w; break;
		case 0x45c: break;
		case 0x470:
			r2_cop_bank = w;
			if (fg_bank != ((r2_cop_bank >> 14) | 4)) { fg_bank = (r2_cop_bank >> 14) | 4; if (foreground_layer) tilemap_mark_all_tiles_dirty(foreground_layer); }
			break;
		case 0x476: r2_cop_dma_adr_rel = w; break;
		case 0x478: if (r2_cop_dma_mode < 0x200) r2_cop_dma_src[r2_cop_dma_mode] = w; break;
		case 0x47a: if (r2_cop_dma_mode < 0x200) r2_cop_dma_size[r2_cop_dma_mode] = w; break;
		case 0x47c: if (r2_cop_dma_mode < 0x200) r2_cop_dma_dst[r2_cop_dma_mode] = w; break;
		case 0x47e: r2_cop_dma_mode = w & 0x1ff; break;
		case 0x4a0: case 0x4a2: case 0x4a4: case 0x4a6: case 0x4a8: case 0x4aa: case 0x4ac:
			idx = (pair & 0x0f) / 2; r2_cop_regs[idx] = (r2_cop_regs[idx] & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x4c0: case 0x4c2: case 0x4c4: case 0x4c6: case 0x4c8: case 0x4ca: case 0x4cc:
			idx = (pair & 0x0f) / 2; r2_cop_regs[idx] = (r2_cop_regs[idx] & 0xffff0000U) | w; break;
		case 0x500: case 0x502: case 0x504: r2_cop_command(pair & 0x7, w); break;
		case 0x6c0: r2_cop_spr_off = w; break;
		case 0x6c2: r2_sprite_prot_src_addr[0] = w; break;
		case 0x6c6: r2_dst1 = w; break;
		case 0x6ca: r2_main_bankswitch(w); break;
		case 0x6cc:
		{
			int nb = (w & 1) << 1;
			int nm = (w & 2) | 1;
			if (bg_bank != nb) { bg_bank = nb; if (background_layer) tilemap_mark_all_tiles_dirty(background_layer); }
			if (mid_bank != nm) { mid_bank = nm; if (midground_layer) tilemap_mark_all_tiles_dirty(midground_layer); }
			break;
		}
		case 0x6d8: r2_sprite_prot_x = w; break;
		case 0x6da: r2_sprite_prot_y = w; break;
		case 0x6dc: r2_cop_spr_maxx = w; break;
		case 0x6de: r2_sprite_prot_src_write(w); break;
		case 0x6fc: r2_cop_dma_trigger(); break;
		case 0x6fe: r2_cop_sort_trigger(w); break;
		default: break;
	}
}

static READ_HANDLER(r2_dst1_r)
{
	return (r2_dst1 >> (offset * 8)) & 0xff;
}

/*
 * Raiden II sprite decryption backport for the MAME 0.78-style core.
 * Algorithm/tables are derived from later MAME's r2crypt implementation.
 * This version deliberately reads/writes logical little-endian dwords so it
 * also behaves correctly on Xbox 360's big-endian Xenon CPU.
 */

static const UINT8 r2_rotate[512] = {
0x11,0x17,0x0d,0x03,0x17,0x1f,0x08,0x1a,0x0f,0x04,0x1e,0x13,0x19,0x0e,0x0e,0x05,
0x06,0x07,0x08,0x08,0x0d,0x18,0x11,0x1a,0x0b,0x06,0x12,0x0c,0x1f,0x0b,0x1c,0x19,
0x00,0x1b,0x0c,0x09,0x1d,0x18,0x1a,0x16,0x1a,0x08,0x03,0x04,0x0f,0x1d,0x16,0x07,
0x1a,0x12,0x01,0x0b,0x00,0x0f,0x1e,0x10,0x09,0x0f,0x10,0x09,0x0a,0x1c,0x0d,0x08,
0x06,0x1a,0x06,0x02,0x11,0x1e,0x0c,0x1c,0x11,0x0f,0x19,0x0a,0x16,0x14,0x18,0x11,
0x0b,0x0d,0x1c,0x1f,0x0d,0x1f,0x0d,0x19,0x0d,0x04,0x19,0x0f,0x06,0x13,0x0c,0x1b,
0x1f,0x12,0x15,0x1a,0x04,0x02,0x06,0x03,0x0a,0x0d,0x12,0x09,0x17,0x1d,0x12,0x10,
0x05,0x07,0x03,0x00,0x14,0x07,0x14,0x1a,0x1c,0x0a,0x10,0x0f,0x0b,0x0c,0x08,0x0f,
0x07,0x00,0x13,0x1c,0x04,0x15,0x0e,0x02,0x17,0x17,0x00,0x03,0x18,0x00,0x02,0x13,
0x14,0x0c,0x01,0x0a,0x15,0x0b,0x0a,0x1c,0x1b,0x06,0x17,0x1d,0x11,0x1f,0x10,0x04,
0x1a,0x01,0x1b,0x13,0x03,0x09,0x09,0x0f,0x0d,0x03,0x15,0x1c,0x04,0x06,0x06,0x0b,
0x04,0x0a,0x1f,0x16,0x11,0x0a,0x05,0x05,0x0c,0x1c,0x10,0x0c,0x11,0x04,0x10,0x1a,
0x06,0x10,0x19,0x06,0x15,0x0f,0x11,0x01,0x10,0x0c,0x1d,0x05,0x1f,0x05,0x12,0x16,
0x02,0x12,0x14,0x0d,0x14,0x0f,0x04,0x07,0x13,0x01,0x11,0x1c,0x1c,0x1d,0x0e,0x06,
0x1d,0x13,0x10,0x06,0x0f,0x02,0x12,0x10,0x1e,0x0c,0x17,0x15,0x0b,0x1f,0x01,0x19,
0x02,0x01,0x07,0x1d,0x13,0x19,0x0f,0x0f,0x10,0x03,0x1e,0x03,0x0d,0x0a,0x0c,0x0d,
0x16,0x1f,0x16,0x1a,0x1c,0x16,0x01,0x03,0x01,0x08,0x14,0x19,0x03,0x1e,0x08,0x02,
0x02,0x1d,0x15,0x00,0x09,0x1d,0x03,0x11,0x11,0x0b,0x1b,0x14,0x01,0x1e,0x11,0x12,
0x1d,0x06,0x0b,0x13,0x1e,0x16,0x0d,0x10,0x11,0x1f,0x1c,0x15,0x0d,0x1a,0x13,0x1f,
0x0e,0x05,0x10,0x06,0x0d,0x1c,0x07,0x19,0x06,0x1d,0x11,0x00,0x1c,0x05,0x0b,0x1d,
0x1c,0x06,0x05,0x1d,0x00,0x13,0x00,0x12,0x1b,0x17,0x1a,0x1b,0x17,0x1c,0x16,0x0a,
0x11,0x15,0x0f,0x0b,0x0f,0x07,0x0e,0x04,0x13,0x00,0x1c,0x05,0x16,0x00,0x1a,0x04,
0x17,0x04,0x08,0x1b,0x05,0x12,0x1d,0x0d,0x02,0x16,0x12,0x0e,0x06,0x08,0x14,0x07,
0x0e,0x0f,0x15,0x13,0x12,0x00,0x1d,0x16,0x1b,0x18,0x1f,0x05,0x12,0x13,0x01,0x0c,
0x12,0x04,0x19,0x13,0x12,0x15,0x07,0x06,0x0a,0x00,0x09,0x14,0x1e,0x03,0x10,0x1b,
0x08,0x1a,0x07,0x02,0x1b,0x0d,0x18,0x13,0x02,0x07,0x1e,0x05,0x15,0x02,0x06,0x18,
0x12,0x09,0x1c,0x07,0x0b,0x02,0x03,0x00,0x18,0x18,0x03,0x0f,0x02,0x0f,0x10,0x09,
0x05,0x18,0x08,0x1b,0x0d,0x10,0x03,0x00,0x0c,0x14,0x1d,0x08,0x02,0x10,0x0b,0x0c,
0x00,0x0d,0x0d,0x0a,0x06,0x1c,0x09,0x19,0x1b,0x14,0x18,0x0f,0x02,0x07,0x05,0x04,
0x1c,0x15,0x18,0x00,0x0b,0x10,0x19,0x1c,0x1b,0x08,0x1d,0x12,0x17,0x1d,0x0c,0x01,
0x03,0x0d,0x03,0x0d,0x15,0x0e,0x16,0x08,0x05,0x11,0x1f,0x03,0x16,0x03,0x0f,0x10,
0x08,0x19,0x18,0x15,0x1f,0x05,0x00,0x09,0x0e,0x05,0x16,0x1b,0x01,0x08,0x08,0x1f
};

static const UINT8 r2_x5[256] = {
0x08,0x09,0x1f,0x0f,0x09,0x09,0x0b,0x1d,0x06,0x13,0x02,0x15,0x02,0x0c,0x0d,0x19,
0x03,0x13,0x0c,0x1f,0x1a,0x18,0x17,0x10,0x0a,0x19,0x15,0x04,0x1f,0x11,0x1c,0x02,
0x0e,0x08,0x06,0x0a,0x07,0x1c,0x10,0x04,0x11,0x0c,0x0a,0x19,0x0a,0x04,0x17,0x07,
0x16,0x1b,0x1d,0x15,0x1d,0x13,0x0e,0x03,0x1a,0x11,0x14,0x14,0x03,0x18,0x07,0x03,
0x08,0x1a,0x02,0x0f,0x0b,0x11,0x1c,0x05,0x19,0x1d,0x05,0x01,0x1f,0x1c,0x1d,0x07,
0x07,0x0c,0x02,0x16,0x0e,0x06,0x0b,0x07,0x01,0x1a,0x09,0x0e,0x0e,0x07,0x0e,0x15,
0x01,0x16,0x13,0x15,0x14,0x07,0x0c,0x1f,0x1f,0x19,0x17,0x12,0x19,0x17,0x0a,0x1f,
0x0c,0x16,0x15,0x1e,0x05,0x14,0x05,0x1c,0x0b,0x0d,0x0c,0x0a,0x05,0x09,0x14,0x02,
0x10,0x02,0x13,0x05,0x12,0x17,0x03,0x0b,0x1b,0x06,0x15,0x0b,0x01,0x0b,0x1b,0x09,
0x10,0x0a,0x1e,0x09,0x08,0x0a,0x04,0x13,0x04,0x12,0x04,0x0f,0x0b,0x0c,0x06,0x07,
0x03,0x18,0x00,0x1e,0x17,0x00,0x16,0x08,0x0d,0x1c,0x09,0x07,0x17,0x18,0x0b,0x0d,
0x11,0x0f,0x14,0x1e,0x1a,0x1b,0x09,0x15,0x03,0x07,0x12,0x16,0x15,0x11,0x16,0x1e,
0x14,0x15,0x00,0x05,0x15,0x18,0x18,0x12,0x18,0x1e,0x06,0x06,0x0c,0x1a,0x04,0x0b,
0x05,0x08,0x04,0x1f,0x0c,0x08,0x0a,0x1f,0x1a,0x16,0x0e,0x1e,0x16,0x18,0x18,0x05,
0x00,0x1a,0x05,0x15,0x19,0x10,0x03,0x0e,0x10,0x1c,0x0a,0x18,0x00,0x16,0x0b,0x05,
0x05,0x15,0x11,0x0a,0x1c,0x00,0x1e,0x1f,0x17,0x12,0x0a,0x1c,0x07,0x04,0x1f,0x1a
};

static const UINT16 r2_x11[256] = {
0x347,0x0f2,0x182,0x58f,0x1f4,0x42c,0x407,0x5f0,0x6df,0x2db,0x585,0x5fe,0x394,0x542,0x3e8,0x574,
0x4ea,0x6d3,0x6b7,0x65b,0x324,0x143,0x22a,0x11d,0x124,0x365,0x7ca,0x3d6,0x1d2,0x7cd,0x6b1,0x4f1,
0x1de,0x674,0x685,0x779,0x264,0x6d8,0x379,0x7ce,0x201,0x73b,0x5c9,0x025,0x338,0x4b2,0x697,0x567,
0x312,0x04e,0x78d,0x492,0x044,0x203,0x437,0x04b,0x729,0x197,0x6e2,0x552,0x517,0x3c9,0x09c,0x3de,
0x2f8,0x259,0x1f0,0x6ce,0x6d6,0x55d,0x223,0x65e,0x7ca,0x330,0x3f7,0x348,0x640,0x26d,0x340,0x2df,
0x752,0x792,0x5b0,0x2fb,0x398,0x75c,0x0a2,0x524,0x538,0x74c,0x1c5,0x5a2,0x522,0x7c3,0x6b3,0x4f0,
0x5ac,0x40b,0x3e0,0x1c8,0x6ff,0x291,0x7c4,0x47c,0x6d9,0x248,0x623,0x78d,0x2cd,0x356,0x12a,0x0bc,
0x582,0x1d8,0x1c6,0x6eb,0x7c2,0x7f9,0x650,0x57d,0x701,0x7e5,0x118,0x1b4,0x4ad,0x2b8,0x2bb,0x765,
0x2d9,0x46a,0x020,0x2da,0x5e4,0x115,0x53c,0x2b4,0x16d,0x0f7,0x633,0x1a6,0x0a0,0x3e6,0x29d,0x77b,
0x558,0x185,0x7b9,0x0b1,0x36e,0x4d3,0x7e2,0x5f9,0x3d2,0x21e,0x0e1,0x2ac,0x0fc,0x0fc,0x66d,0x7b5,
0x4af,0x627,0x0f4,0x621,0x58f,0x3d7,0x1bc,0x10a,0x458,0x259,0x451,0x770,0x107,0x134,0x162,0x32f,
0x5cf,0x6c9,0x670,0x2d4,0x0da,0x739,0x30c,0x62f,0x4af,0x0e2,0x3e3,0x65c,0x214,0x066,0x47d,0x2f2,
0x729,0x566,0x450,0x3f2,0x35d,0x593,0x593,0x532,0x008,0x270,0x479,0x358,0x6f3,0x7ed,0x240,0x587,
0x581,0x00f,0x750,0x4d8,0x1ab,0x100,0x47f,0x34f,0x497,0x240,0x769,0x76f,0x705,0x375,0x684,0x273,
0x01f,0x268,0x2cc,0x2d7,0x5d4,0x284,0x40c,0x5e8,0x7c1,0x281,0x518,0x4b0,0x136,0x73b,0x3ea,0x023,
0x1c1,0x7de,0x106,0x275,0x1e1,0x503,0x30a,0x271,0x4f8,0x52b,0x266,0x375,0x024,0x399,0x672,0x6f8
};

static UINT32 r2_partial_carry_sum32(UINT32 add1, UINT32 add2, UINT32 carry_mask)
{
	int i;
	UINT32 res = 0;
	int carry = 0;
	for (i = 0; i < 32; ++i)
	{
		int bit = ((add1 >> i) & 1) + ((add2 >> i) & 1) + carry;
		res |= (UINT32)(bit & 1) << i;
		carry = ((carry_mask >> i) & 1) ? (bit >> 1) : 0;
	}
	if (carry) res ^= 1;
	return res;
}

static UINT32 r2_yrot(UINT32 v, int r)
{
	/* Rotation table includes zero; shifting by 32 is undefined in C and
	   does not have portable Xenon/compiler semantics. */
	r &= 31;
	return r ? (v << r) | (v >> (32 - r)) : v;
}

static UINT16 r2_gm(int i4)
{
	UINT16 x = 0;
	int i;
	for (i = 0; i < 4; ++i)
		if ((i4 >> i) & 1) x ^= (UINT16)(0x0f << (i << 2));
	return x;
}

static UINT32 r2_bitswap32(UINT32 v)
{
	static const UINT8 b[32] = {25,28,15,19,6,0,3,24,11,1,2,30,16,7,22,17,31,14,23,9,27,18,4,10,13,20,5,12,8,29,26,21};
	UINT32 r = 0;
	int i;
	for (i = 0; i < 32; ++i) r |= ((v >> b[i]) & 1U) << (31 - i);
	return r;
}

static UINT16 r2_bitswap16(UINT16 v)
{
	static const UINT8 b[16] = {0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15};
	UINT16 r = 0;
	int i;
	for (i = 0; i < 16; ++i) r |= (UINT16)(((v >> b[i]) & 1U) << (15 - i));
	return r;
}

static UINT32 r2_core_decrypt(UINT32 ciphertext, int i1, int i2, int i3, int i4)
{
	UINT32 v1 = r2_bitswap32(r2_yrot(ciphertext, r2_rotate[i1]));
	UINT16 x1low = (UINT16)((r2_x5[i2] << 11) ^ r2_x11[i3] ^ r2_gm(i4));
	UINT32 x1 = (UINT32)x1low | ((UINT32)r2_bitswap16(x1low) << 16);
	return r2_partial_carry_sum32(v1, x1 ^ 0x60860000U, 0x176c91a8U) ^ 0x0f488000U;
}

static UINT32 r2_read32le_buf(const UINT8 *p)
{
	return (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

static void r2_write32le_buf(UINT8 *p, UINT32 v)
{
	p[0] = (UINT8)v;
	p[1] = (UINT8)(v >> 8);
	p[2] = (UINT8)(v >> 16);
	p[3] = (UINT8)(v >> 24);
}

static int raiden2_prepare_and_decrypt_sprites(UINT8 *rom)
{
	UINT8 *tmp;
	UINT32 i;

	/* Old ROM sets load obj1..obj4 contiguously. Repack to the 32-bit
	   word-interleaved layout used by the later working driver. */
	tmp = (UINT8 *)malloc(0x800000);
	if (!tmp) return 1;
	memcpy(tmp, rom, 0x800000);

	for (i = 0; i < 0x200000U / 2U; ++i)
	{
		rom[0x000000 + i * 4 + 0] = tmp[0x000000 + i * 2 + 0];
		rom[0x000000 + i * 4 + 1] = tmp[0x000000 + i * 2 + 1];
		rom[0x000000 + i * 4 + 2] = tmp[0x200000 + i * 2 + 0];
		rom[0x000000 + i * 4 + 3] = tmp[0x200000 + i * 2 + 1];
		rom[0x400000 + i * 4 + 0] = tmp[0x400000 + i * 2 + 0];
		rom[0x400000 + i * 4 + 1] = tmp[0x400000 + i * 2 + 1];
		rom[0x400000 + i * 4 + 2] = tmp[0x600000 + i * 2 + 0];
		rom[0x400000 + i * 4 + 3] = tmp[0x600000 + i * 2 + 1];
	}
	free(tmp);

	/* Decrypt logical little-endian dwords explicitly. */
	for (i = 0; i < 0x800000U / 4U; ++i)
	{
		UINT32 v = r2_read32le_buf(rom + i * 4);
		v = r2_core_decrypt(v,
			(int)((i & 0xff) ^ ((i >> 15) & 1) ^ (((i >> 20) & 1) << 8)),
			(int)((i & 0xff) ^ ((i >> 15) & 1)),
			(int)((i >> 8) & 0xff),
			(int)((i >> 16) & 0x0f));
		r2_write32le_buf(rom + i * 4, v);
	}
	return 0;
}


/* -----------------------------------------------------------------------
 * Video
 * ----------------------------------------------------------------------- */
 

static UINT16 r2_spr_word(const UINT8 *p)
{
	return (UINT16)(p[0] | ((UINT16)p[1] << 8));
}

static void r2_draw_one_sprite(struct mame_bitmap *bitmap, const struct rectangle *cliprect,
                               int tile, int color, int flipx, int flipy, int sx, int sy)
{
	/* Avoid general drawgfx setup for invisible wraparound copies. */
	if (sx > cliprect->max_x || sy > cliprect->max_y ||
	    sx + 15 < cliprect->min_x || sy + 15 < cliprect->min_y) return;
/*		
#if RAIDEN2_DEBUG
	if(sx<=cliprect->max_x && sy<=cliprect->max_y && sx+16>cliprect->min_x && sy+16>cliprect->min_y &&
	   Machine->gfx[2]->pen_usage && (Machine->gfx[2]->pen_usage[tile&0xffff]&~(1U<<15))) r2_trace.tiles[4]++;
#endif
*/
	drawgfx(bitmap, Machine->gfx[2], tile & 0xffff, color & 0x3f,
	        flipx, flipy, sx, sy, cliprect, TRANSPARENCY_PEN, 15);
}

static void draw_sprites(struct mame_bitmap *bitmap, const struct rectangle *cliprect, int priority)
{
	/* The SEI25x reads the snapshot latched by the write to 0x68e, not
	 * the CPU's work list. Live RAM can contain a new code/size but the
	 * previous sprite's coordinates while the COP builds the next list. */
	const UINT8 *base = buffered_spriteram;
	const UINT8 *source;

	if (!base || spriteram_size < 8 || !Machine->gfx[2]) return;
	if (r2_video_enable() & 0x10) return;

	/* COP channel 0x14 is tilemap DMA, not a sprite-list length register.
	 * Walk the complete latched sprite RAM, as on the later MAME driver. */
	source = base + ((spriteram_size - 8) & ~(size_t)7);

	while (source >= base)
	{
		UINT16 a = r2_spr_word(source + 0);
		int tile_number = r2_spr_word(source + 2);
		int sx = r2_spr_word(source + 4);
		int sy = r2_spr_word(source + 6);
		int ytlim = ((a >> 12) & 7) + 1;
		int xtlim = ((a >> 8) & 7) + 1;
		int xflip = (a >> 15) & 1;
		int yflip = (a >> 11) & 1;
		int colr = a & 0x3f;
		int pri = (a >> 6) & 3;
		int xstep = 16, ystep = 16;
		int xt, yt;

		/* Code zero marks an empty/control entry (including the sprite-RAM
		 * header), not drawable tile zero. Match MAME's SEI25x sprite device
		 * and reject the whole entry before expanding its size fields. Drawing
		 * it can leave a block at (0,0), the lower-left corner after ROT270. */
		if (tile_number != 0 && pri == priority)
		{
			if (xflip) { ystep = -16; sy += ytlim * 16 - 16; }
			if (yflip) { xstep = -16; sx += xtlim * 16 - 16; }

			for (xt = 0; xt < xtlim; ++xt)
			{
				for (yt = 0; yt < ytlim; ++yt)
				{
					int x = (sx + xstep * xt) & 0x1ff;
					int y = (sy + ystep * yt) & 0x1ff;
					r2_draw_one_sprite(bitmap, cliprect, tile_number, colr, yflip, xflip, x, y);
					r2_draw_one_sprite(bitmap, cliprect, tile_number, colr, yflip, xflip, x - 0x200, y);
					r2_draw_one_sprite(bitmap, cliprect, tile_number, colr, yflip, xflip, x, y - 0x200);
					r2_draw_one_sprite(bitmap, cliprect, tile_number, colr, yflip, xflip, x - 0x200, y - 0x200);
					tile_number++;
				}
			}
		}

		if (source == base) break;
		source -= 8;
	}
}

WRITE_HANDLER( raiden2_background_w )
{
	if (back_data[offset] == (UINT8)data) return;
	back_data[offset] = data;
	tilemap_mark_tile_dirty(background_layer, offset / 2);
}

WRITE_HANDLER( raiden2_midground_w )
{
	if (mid_data[offset] == (UINT8)data) return;
	mid_data[offset] = data;
	tilemap_mark_tile_dirty(midground_layer, offset / 2);
}

WRITE_HANDLER( raiden2_foreground_w )
{
	if (fore_data[offset] == (UINT8)data) return;
	fore_data[offset] = data;
	tilemap_mark_tile_dirty(foreground_layer, offset / 2);
}

WRITE_HANDLER( raiden2_text_w )
{
	if (videoram[offset] == (UINT8)data) return;
	videoram[offset] = data;
	tilemap_mark_tile_dirty(text_layer, offset / 2);
}

static void get_back_tile_info(int tile_index)
{
	int offs = tile_index * 2;
	int v = back_data[offs] | (back_data[offs + 1] << 8);
	int color = (v >> 12) & 0x0f;
	int tile = (v & 0x0fff) | (bg_bank << 12);
	SET_TILE_INFO(1, tile, color, 0)
}

static void get_mid_tile_info(int tile_index)
{
	int offs = tile_index * 2;
	int v = mid_data[offs] | (mid_data[offs + 1] << 8);
	int color = ((v >> 12) & 0x0f) + 0x20;
	int tile = (v & 0x0fff) | (mid_bank << 12);
	SET_TILE_INFO(1, tile, color, 0)
}

static void get_fore_tile_info(int tile_index)
{
	int offs = tile_index * 2;
	int v = fore_data[offs] | (fore_data[offs + 1] << 8);
	int color = ((v >> 12) & 0x0f) + 0x10;
	int tile = (v & 0x0fff) | (fg_bank << 12);
	SET_TILE_INFO(1, tile, color, 0)
}

static void get_text_tile_info(int tile_index)
{
	int offs = tile_index * 2;
	int v = videoram[offs] | (videoram[offs + 1] << 8);
	int color = (v >> 12) & 0x0f;
	int tile = (v & 0x0fff) | (tx_bank << 12);
	SET_TILE_INFO(0, tile, color, 0)
}

VIDEO_START(raiden2)
{
	/* Generic video allocates/registers the 4 KiB snapshot for save states.
	 * Do not display uninitialised data before the first hardware latch. */
	memset(buffered_spriteram, 0, spriteram_size);
	text_layer       = tilemap_create(get_text_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8, 8, 64, 32);
	background_layer = tilemap_create(get_back_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	midground_layer  = tilemap_create(get_mid_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	foreground_layer = tilemap_create(get_fore_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	if (!text_layer || !background_layer || !midground_layer || !foreground_layer) return 1;
	tilemap_set_transparent_pen(background_layer, 15);
	tilemap_set_transparent_pen(midground_layer, 15);
	tilemap_set_transparent_pen(foreground_layer, 15);
	tilemap_set_transparent_pen(text_layer, 15);
	return 0;
}

VIDEO_UPDATE(raiden2)
{
	int sx, sy;
/*	
#if RAIDEN2_DEBUG
	r2_trace.tiles[4]=0;
#endif
*/
	fillbitmap(bitmap, get_black_pen(), cliprect);

	sx = r2_scroll[0] | (r2_scroll[1] << 8); sy = r2_scroll[2] | (r2_scroll[3] << 8);
	tilemap_set_scrollx(background_layer, 0, sx & 0x1ff); tilemap_set_scrolly(background_layer, 0, sy & 0x1ff);
	sx = r2_scroll[4] | (r2_scroll[5] << 8); sy = r2_scroll[6] | (r2_scroll[7] << 8);
	tilemap_set_scrollx(midground_layer, 0, sx & 0x1ff); tilemap_set_scrolly(midground_layer, 0, sy & 0x1ff);
	sx = r2_scroll[8] | (r2_scroll[9] << 8); sy = r2_scroll[10] | (r2_scroll[11] << 8);
	tilemap_set_scrollx(foreground_layer, 0, sx & 0x1ff); tilemap_set_scrolly(foreground_layer, 0, sy & 0x1ff);

	/* Priority order follows the later working implementation. */
	draw_sprites(bitmap, cliprect, 0);
	if (!(r2_video_enable() & 0x01)) tilemap_draw(bitmap, cliprect, background_layer, 0, 0);
	draw_sprites(bitmap, cliprect, 1);
	if (!(r2_video_enable() & 0x02)) tilemap_draw(bitmap, cliprect, midground_layer, 0, 0);
	draw_sprites(bitmap, cliprect, 2);
	if (!(r2_video_enable() & 0x04)) tilemap_draw(bitmap, cliprect, foreground_layer, 0, 0);
	draw_sprites(bitmap, cliprect, 3);
	if (!(r2_video_enable() & 0x08)) tilemap_draw(bitmap, cliprect, text_layer, 0, 0);
//	r2_debug_video(bitmap, cliprect);
}

/* -----------------------------------------------------------------------
 * Main CPU memory
 * ----------------------------------------------------------------------- */

static READ_HANDLER(r2_openbus_r) { return 0xff; }

static READ_HANDLER(r2_inputs_r)
{
	int value = 0xff;
	switch (offset) {
	case 0: value=readinputport(2); break;
	case 1: value=readinputport(3); break;
	case 4: value=readinputport(0); break;
	case 5: value=readinputport(1); break;
	case 12: value=readinputport(4); break;
	}
//	r2_debug_access(0x740+offset, (value&0xff)<<((offset&1)*8), 0);
	return value;
}

/* Raiden II spreads eight Seibu registers across 32 V30 byte addresses.
   Modern MAME divides its WORD offset by two; our BYTE offset must divide
   by four. Keep the generic Seibu handler unchanged for other games. */
static READ_HANDLER(r2_sound_main_r)
{
 UINT16 word = seibu_main_word_r((offset >> 2) & 7, 0);
 int value = (word >> ((offset & 1) * 8)) & 0xff;
//r2_debug_access(0x700 + offset, value << ((offset & 1) * 8), 0);
 return value;
}

static WRITE_HANDLER(r2_sound_main_w)
{
 //r2_debug_access(0x700 + offset, (data & 0xff) << ((offset & 1) * 8), 1);
 /* Only the low byte is wired. Bit 1 of the address is a mirror. */
 if (!(offset & 1)) seibu_main_word_w((offset >> 2) & 7, data & 0xff, 0xff00);
}

static MEMORY_READ_START( raiden2_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00400, 0x006ff, r2_cop_r },
	{ 0x00700, 0x0071f, r2_sound_main_r },
	{ 0x00740, 0x0074d, r2_inputs_r },
	{ 0x00762, 0x00763, r2_dst1_r },
	{ 0x00000, 0x007ff, MRA_RAM }, /* unassigned low I/O shadow, as on the Vez map */
	{ 0x00800, 0x0bfff, MRA_RAM },
	{ 0x0c000, 0x0cfff, MRA_RAM },
	{ 0x0d000, 0x0d7ff, MRA_RAM },
	{ 0x0d800, 0x0dfff, MRA_RAM },
	{ 0x0e000, 0x0e7ff, MRA_RAM },
	{ 0x0e800, 0x0f7ff, MRA_RAM },
	{ 0x0f800, 0x0ffff, MRA_RAM },
	{ 0x10000, 0x1efff, MRA_RAM },
	{ 0x1f000, 0x1ffff, MRA_RAM },
	{ 0x20000, 0x2ffff, MRA_BANK3 },
	{ 0x30000, 0x3ffff, MRA_BANK4 },
	{ 0x40000, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( raiden2_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	/* Specific register must precede the overlapping COP range: the old
	 * memory-map builder gives the first matching entry precedence. */
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x00400, 0x006ff, r2_cop_w },
	{ 0x00700, 0x0071f, r2_sound_main_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0bfff, MWA_RAM },
	{ 0x0c000, 0x0cfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0d000, 0x0d7ff, raiden2_background_w, &back_data },
	{ 0x0d800, 0x0dfff, raiden2_foreground_w, &fore_data },
	{ 0x0e000, 0x0e7ff, raiden2_midground_w, &mid_data },
	{ 0x0e800, 0x0f7ff, raiden2_text_w, &videoram },
	{ 0x0f800, 0x0ffff, MWA_RAM },
	{ 0x10000, 0x1efff, MWA_RAM },
	{ 0x1f000, 0x1ffff, paletteram_xBBBBBGGGGGRRRRR_w, &paletteram },
	{ 0x20000, 0xfffff, MWA_ROM },
MEMORY_END

/* -----------------------------------------------------------------------
 * Sound CPU: YM2151 + dual OKIM6295
 * ----------------------------------------------------------------------- */
 


static READ_HANDLER(r2_sound_z80_r)
{
	int value;
	if(offset<2) value=seibu_soundlatch_r(offset);
	else if(offset==2) value=seibu_main_data_pending_r(0);
	else value=readinputport(5);
//	r2_debug_sound_access(0x4010+offset,value,0);
	return value;
}

static WRITE_HANDLER(r2_sound_z80_reply_w)
{
	seibu_main_data_w(offset,data);
//	r2_debug_sound_access(0x4018+offset,data,1);
}

static WRITE_HANDLER(r2_sound_z80_pending_w)
{
	seibu_pending_w(offset,data);
//	r2_debug_sound_access(0x4000,data,1);
}

static MEMORY_READ_START( raiden2_sound_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x4009, 0x4009, YM2151_status_port_0_r },
	{ 0x4010, 0x4013, r2_sound_z80_r },
	{ 0x6000, 0x6000, OKIM6295_status_0_r },
	{ 0x6002, 0x6002, OKIM6295_status_1_r },
	{ 0x8000, 0xffff, MRA_BANK1 },
MEMORY_END

static MEMORY_WRITE_START( raiden2_sound_writemem )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x27ff, MWA_RAM },
	{ 0x4000, 0x4000, r2_sound_z80_pending_w },
	{ 0x4001, 0x4001, seibu_irq_clear_w },
	{ 0x4002, 0x4002, seibu_rst10_ack_w },
	{ 0x4003, 0x4003, seibu_rst18_ack_w },
	{ 0x4007, 0x4007, seibu_bank_w },
	{ 0x4008, 0x4008, YM2151_register_port_0_w },
	{ 0x4009, 0x4009, YM2151_data_port_0_w },
	{ 0x4018, 0x4019, r2_sound_z80_reply_w },
	{ 0x401a, 0x401a, seibu_bank_w },
	{ 0x401b, 0x401b, seibu_coin_w },
	{ 0x6000, 0x6000, OKIM6295_data_0_w },
	{ 0x6002, 0x6002, OKIM6295_data_1_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

static struct YM2151interface raiden2_ym2151_interface =
{
	1,
	28636360/8,
	{ YM3012_VOL(70,MIXER_PAN_LEFT,70,MIXER_PAN_RIGHT) },
	{ seibu_ym2151_irqhandler }
};

static struct OKIM6295interface raiden2_oki_interface =
{
	2,
	{ (28636360/28)/132, (28636360/28)/132 },
	{ REGION_SOUND1, REGION_SOUND2 },
	{ 40, 40 }
};

/* -----------------------------------------------------------------------
 * Inputs
 * ----------------------------------------------------------------------- */

INPUT_PORTS_START( raiden2 )
	PORT_START /* P1 low byte */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1)
	PORT_BIT(0xc0, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START /* P2 high byte of modern P1_P2 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2)
	PORT_BIT(0xc0, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START /* DSW A */
	PORT_DIPNAME(0x07, 0x07, DEF_STR(Coin_A))
	PORT_DIPSETTING(0x01, DEF_STR(4C_1C))
	PORT_DIPSETTING(0x02, DEF_STR(3C_1C))
	PORT_DIPSETTING(0x04, DEF_STR(2C_1C))
	PORT_DIPSETTING(0x07, DEF_STR(1C_1C))
	PORT_DIPSETTING(0x06, DEF_STR(1C_2C))
	PORT_DIPSETTING(0x05, DEF_STR(1C_3C))
	PORT_DIPSETTING(0x03, DEF_STR(1C_4C))
	PORT_DIPSETTING(0x00, DEF_STR(Free_Play))
	PORT_DIPNAME(0x38, 0x38, DEF_STR(Coin_B))
	PORT_DIPSETTING(0x08, DEF_STR(4C_1C))
	PORT_DIPSETTING(0x10, DEF_STR(3C_1C))
	PORT_DIPSETTING(0x20, DEF_STR(2C_1C))
	PORT_DIPSETTING(0x38, DEF_STR(1C_1C))
	PORT_DIPSETTING(0x30, DEF_STR(1C_2C))
	PORT_DIPSETTING(0x28, DEF_STR(1C_3C))
	PORT_DIPSETTING(0x18, DEF_STR(1C_4C))
	PORT_DIPSETTING(0x00, DEF_STR(Free_Play))
	PORT_DIPNAME(0x40, 0x40, "Starting Coin")
	PORT_DIPSETTING(0x40, "Normal")
	PORT_DIPSETTING(0x00, "X 2")
	PORT_DIPNAME(0x80, 0x80, DEF_STR(Flip_Screen))
	PORT_DIPSETTING(0x80, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))

	PORT_START /* DSW B */
	PORT_DIPNAME(0x03, 0x03, DEF_STR(Difficulty))
	PORT_DIPSETTING(0x03, "Normal")
	PORT_DIPSETTING(0x02, "Easy")
	PORT_DIPSETTING(0x01, "Hard")
	PORT_DIPSETTING(0x00, "Very Hard")
	PORT_DIPNAME(0x0c, 0x0c, DEF_STR(Lives))
	PORT_DIPSETTING(0x00, "1")
	PORT_DIPSETTING(0x04, "4")
	PORT_DIPSETTING(0x08, "2")
	PORT_DIPSETTING(0x0c, "3")
	PORT_DIPNAME(0x30, 0x30, DEF_STR(Bonus_Life))
	PORT_DIPSETTING(0x30, "200000 500000")
	PORT_DIPSETTING(0x20, "400000 1000000")
	PORT_DIPSETTING(0x10, "1000000 3000000")
	PORT_DIPSETTING(0x00, "None")
	PORT_DIPNAME(0x40, 0x40, "Demo Sound")
	PORT_DIPSETTING(0x00, DEF_STR(Off))
	PORT_DIPSETTING(0x40, DEF_STR(On))
	PORT_DIPNAME(0x80, 0x80, "Test Mode")
	PORT_DIPSETTING(0x80, DEF_STR(Off))
	PORT_DIPSETTING(0x00, DEF_STR(On))

	PORT_START /* SYSTEM at 0x74c */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_START1)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_START2)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_SERVICE1)
	PORT_BIT(0xf0, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START /* coins read by sound CPU */
	PORT_BIT_IMPULSE(0x01, IP_ACTIVE_LOW, IPT_COIN1, 4)
	PORT_BIT_IMPULSE(0x02, IP_ACTIVE_LOW, IPT_COIN2, 4)
	PORT_BIT(0xfc, IP_ACTIVE_LOW, IPT_UNUSED)
INPUT_PORTS_END

/* -----------------------------------------------------------------------
 * Graphics decode
 * ----------------------------------------------------------------------- */

static struct GfxLayout raiden2_charlayout =
{
	8,8,4096,4,
	{8,12,0,4},
	{3,2,1,0,19,18,17,16},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32},
	32*8
};

static struct GfxLayout raiden2_tilelayout =
{
	16,16,0x8000,4,
	{8,12,0,4},
	{3,2,1,0,19,18,17,16,3+64*8,2+64*8,1+64*8,0+64*8,19+64*8,18+64*8,17+64*8,16+64*8},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,8*32,9*32,10*32,11*32,12*32,13*32,14*32,15*32},
	128*8
};

static struct GfxLayout raiden2_spritelayout =
{
	16,16,0x10000,4,
	{0,1,2,3},
	{4,0,12,8,20,16,28,24,36,32,44,40,52,48,60,56},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64,8*64,9*64,10*64,11*64,12*64,13*64,14*64,15*64},
	16*16*4
};

static struct GfxDecodeInfo raiden2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &raiden2_charlayout,   0x700, 0x10 },
	{ REGION_GFX2, 0x00000, &raiden2_tilelayout,   0x400, 0x40 },
	{ REGION_GFX3, 0x00000, &raiden2_spritelayout, 0x000, 0x40 },
	{ -1 }
};

/* -----------------------------------------------------------------------
 * Machine
 * ----------------------------------------------------------------------- */

static INTERRUPT_GEN( raiden2_interrupt )
{
//	r2_debug_frame();
	cpu_set_irq_line_and_vector(cpu_getactivecpu(), 0, HOLD_LINE, 0xc0/4);
}

static void r2_state_postload(void)
{
	r2_main_bankswitch(r2_prg_bank);
	if (background_layer) tilemap_mark_all_tiles_dirty(background_layer);
	if (midground_layer) tilemap_mark_all_tiles_dirty(midground_layer);
	if (foreground_layer) tilemap_mark_all_tiles_dirty(foreground_layer);
	if (text_layer) tilemap_mark_all_tiles_dirty(text_layer);
}

static void r2_state_save_register(void)
{
	int i;
	state_save_register_int("raiden2", 0, "bg_bank", &bg_bank);
	state_save_register_int("raiden2", 0, "mid_bank", &mid_bank);
	state_save_register_int("raiden2", 0, "fg_bank", &fg_bank);
	state_save_register_int("raiden2", 0, "tx_bank", &tx_bank);
	state_save_register_UINT8("raiden2", 0, "scroll", r2_scroll, 12);
	state_save_register_UINT16("raiden2", 0, "layer_enable", &r2_layer_enable, 1);
	state_save_register_UINT16("raiden2", 0, "prg_bank", &r2_prg_bank, 1);

	state_save_register_UINT8("raiden2_cop", 0, "ram", r2_copram, sizeof(r2_copram));
	state_save_register_UINT32("raiden2_cop", 0, "regs", r2_cop_regs, 8);
	state_save_register_UINT32("raiden2_cop", 0, "itoa", &r2_cop_itoa, 1);
	state_save_register_UINT16("raiden2_cop", 0, "status", &r2_cop_status, 1);
	state_save_register_UINT16("raiden2_cop", 0, "scale", &r2_cop_scale, 1);
	state_save_register_UINT16("raiden2_cop", 0, "itoa_digit_count", &r2_cop_itoa_digit_count, 1);
	state_save_register_UINT8("raiden2_cop", 0, "itoa_digits", r2_cop_itoa_digits, 10);
	state_save_register_UINT16("raiden2_cop", 0, "angle", &r2_cop_angle, 1);
	state_save_register_UINT16("raiden2_cop", 0, "distance", &r2_cop_dist, 1);
	state_save_register_UINT16("raiden2_cop", 0, "angle_target", &r2_cop_angle_target, 1);
	state_save_register_UINT16("raiden2_cop", 0, "angle_step", &r2_cop_angle_step, 1);
	state_save_register_UINT16("raiden2_cop", 0, "dma_mode", &r2_cop_dma_mode, 1);
	state_save_register_UINT16("raiden2_cop", 0, "dma_src", r2_cop_dma_src, 0x200);
	state_save_register_UINT16("raiden2_cop", 0, "dma_dst", r2_cop_dma_dst, 0x200);
	state_save_register_UINT16("raiden2_cop", 0, "dma_size", r2_cop_dma_size, 0x200);
	state_save_register_UINT16("raiden2_cop", 0, "dma_v1", &r2_cop_dma_v1, 1);
	state_save_register_UINT16("raiden2_cop", 0, "dma_v2", &r2_cop_dma_v2, 1);
	state_save_register_UINT16("raiden2_cop", 0, "dma_adr_rel", &r2_cop_dma_adr_rel, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprites_start", &r2_sprites_cur_start, 1);
	state_save_register_UINT16("raiden2_cop", 0, "brightness", &r2_pal_brightness_val, 1);
	state_save_register_UINT16("raiden2_cop", 0, "bank", &r2_cop_bank, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprite_x", &r2_sprite_prot_x, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprite_y", &r2_sprite_prot_y, 1);
	state_save_register_UINT16("raiden2_cop", 0, "dst1", &r2_dst1, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprite_maxx", &r2_cop_spr_maxx, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprite_off", &r2_cop_spr_off, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sprite_src", r2_sprite_prot_src_addr, 2);
	state_save_register_UINT16("raiden2_cop", 0, "hit_status", &r2_cop_hit_status, 1);
	state_save_register_UINT16("raiden2_cop", 0, "hit_base", &r2_cop_hit_baseadr, 1);
	state_save_register_UINT16("raiden2_cop", 0, "hit_value_status", &r2_cop_hit_val_stat, 1);
	state_save_register_INT16("raiden2_cop", 0, "hit_values", r2_cop_hit_val, 3);
	state_save_register_UINT32("raiden2_cop", 0, "sort_ram", &r2_cop_sort_ram_addr, 1);
	state_save_register_UINT32("raiden2_cop", 0, "sort_lookup", &r2_cop_sort_lookup, 1);
	state_save_register_UINT16("raiden2_cop", 0, "sort_param", &r2_cop_sort_param, 1);
	for (i = 0; i < 2; ++i) {
		state_save_register_INT16("raiden2_collision", i, "pos", r2_collision[i].pos, 3);
		state_save_register_INT8("raiden2_collision", i, "delta", r2_collision[i].dx, 3);
		state_save_register_UINT8("raiden2_collision", i, "size", r2_collision[i].size, 3);
		state_save_register_int("raiden2_collision", i, "allow_swap", &r2_collision[i].allow_swap);
		state_save_register_UINT16("raiden2_collision", i, "flags_swap", &r2_collision[i].flags_swap, 1);
		state_save_register_UINT32("raiden2_collision", i, "sprite_address", &r2_collision[i].spradr, 1);
		state_save_register_INT16("raiden2_collision", i, "minimum", r2_collision[i].min, 3);
		state_save_register_INT16("raiden2_collision", i, "maximum", r2_collision[i].max, 3);
	}
	state_save_register_func_postload(r2_state_postload);
}

static MACHINE_INIT(raiden2)
{
	machine_init_seibu_sound_1();
	seibu_bank_w(0, 0);

	bg_bank = 0;
	mid_bank = 1;
	fg_bank = 6;
	tx_bank = 0;
	memset(r2_scroll, 0, sizeof(r2_scroll));
	r2_layer_enable = 0;
	r2_prg_bank = 0;
	r2_reset_cop();
	if (buffered_spriteram)
		memset(buffered_spriteram, 0, spriteram_size);

	/* Working later driver starts on entries 2/3.  Use BANK3/BANK4 so the
	   sound CPU can keep BANK1 for the Seibu sound ROM. */
	r2_main_bankswitch(0);
//	r2_debug_reset();
}

static MACHINE_DRIVER_START( raiden2 )
	MDRV_CPU_ADD(V30, 32000000/2)
	MDRV_CPU_MEMORY(raiden2_readmem, raiden2_writemem)
	MDRV_CPU_VBLANK_INT(raiden2_interrupt, 1)

	MDRV_CPU_ADD(Z80, 28636360/8)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(raiden2_sound_readmem, raiden2_sound_writemem)

	MDRV_FRAMES_PER_SECOND(55.47)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_MACHINE_INIT(raiden2)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(raiden2_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(raiden2)
	MDRV_VIDEO_UPDATE(raiden2)

	MDRV_SOUND_ADD(YM2151, raiden2_ym2151_interface)
	MDRV_SOUND_ADD(OKIM6295, raiden2_oki_interface)
MACHINE_DRIVER_END

/* -----------------------------------------------------------------------
 * ROMs
 * ----------------------------------------------------------------------- */

ROM_START( raiden2 )
	ROM_REGION(0x200000, REGION_CPU1, 0)
	ROM_LOAD16_BYTE("prg0.u0211", 0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32))
	ROM_RELOAD(                 0x100000, 0x80000)
	ROM_LOAD16_BYTE("prg1.u0212", 0x000001, 0x80000, CRC(4609b5f2) SHA1(272d2aa75b8ea4d133daddf42c4fc9089093df2e))
	ROM_RELOAD(                 0x100001, 0x80000)

	ROM_REGION(0x20000, REGION_CPU2, 0)
	ROM_LOAD("snd.u1110", 0x00000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302))
	ROM_CONTINUE(   0x10000, 0x08000)
	ROM_COPY(REGION_CPU2, 0x00000, 0x18000, 0x08000)
	
	// Common Raiden II soldered mask ROMs below
  ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )


	ROM_REGION(0x020000, REGION_GFX1, ROMREGION_DISPOSE)
	ROM_LOAD("seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24))
	

	ROM_REGION(0x400000, REGION_GFX2, ROMREGION_DISPOSE)
	ROM_LOAD("raiden_2_seibu_bg-1.u075", 0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec))
	ROM_LOAD("raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77))

	/* Keep old-set contiguous loading. DRIVER_INIT explicitly repacks and
	   decrypts this region before MAME decodes the graphics. */
	ROM_REGION(0x800000, REGION_GFX3, ROMREGION_DISPOSE)
	ROM_LOAD("raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1))
	ROM_LOAD("raiden_2_seibu_obj-2.u082",  0x200000, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b))
	ROM_LOAD("raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126))
	ROM_LOAD("raiden_2_seibu_obj-4.u0836", 0x600000, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f))

	ROM_REGION(0x100000, REGION_SOUND1, 0)
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	ROM_REGION(0x100000, REGION_SOUND2, 0)
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raidendx )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1d.4n",   0x000000, 0x80000, CRC(14d725fc) SHA1(f12806f64f069fdc4ee29b309a32f7ca00b36f93) )
	ROM_LOAD32_BYTE("2d.4p",   0x000001, 0x80000, CRC(5e7e45cb) SHA1(94eff893b5335c522f1c063c3175b9bac87b0a25) )
	ROM_LOAD32_BYTE("3d.6n",   0x000002, 0x80000, CRC(f0a47e67) SHA1(8cbd21993077b2e01295db6e343cae9e0e4bfefe) )
	ROM_LOAD32_BYTE("4d.6p",   0x000003, 0x80000, CRC(2a2003e8) SHA1(f239b351759babe4683d16e745a5ac2f3c2ab06b) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "dx_5.5b",  0x00000, 0x08000,  CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(   0x10000, 0x08000)
	ROM_COPY(REGION_CPU2, 0x00000, 0x18000, 0x08000)
	
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.6s",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION(0x020000, REGION_GFX1, ROMREGION_DISPOSE)
	ROM_LOAD( "dx_7.4s",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION(0x400000, REGION_GFX2, ROMREGION_DISPOSE)
	ROM_LOAD( "dx_back1.1s",   0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back2.2s",   0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	/* Keep old-set contiguous loading. DRIVER_INIT explicitly repacks and
	   decrypts this region before MAME decodes the graphics. */
	ROM_REGION(0x800000, REGION_GFX3, ROMREGION_DISPOSE)
	ROM_LOAD( "obj1",        0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD( "obj2",        0x200000, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD( "dx_obj3.4k",  0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD( "dx_obj4.6k",  0x600000, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "dx_6.3b",   0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "dx_pcm.3a", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Shared with original Raiden 2 */
ROM_END

static DRIVER_INIT(raiden2)
{
	UINT8 *gfx = memory_region(REGION_GFX3);
	r2_bank_rom = auto_malloc(0x40000);
	if (r2_bank_rom) memcpy(r2_bank_rom, memory_region(REGION_CPU1), 0x40000);
	else r2_bank_rom = memory_region(REGION_CPU1) + 0x100000; /* untouched ROM_RELOAD mirror */
/*	
#if RAIDEN2_DEBUG
	r2_log_path[0] = 0;
	r2_frame_number = r2_reset_count = r2_last_draws = 0;
	r2_ever_lit = r2_was_black = 0;
	r2_old_coin = r2_old_start = 0xff;
#endif
*/
	if (raiden2_prepare_and_decrypt_sprites(gfx) != 0)
		log_cb(RETRO_LOG_DEBUG, LOGPRE "Raiden2: sprite decryption allocation failed\n");
	r2_state_save_register();
	seibu_sound_state_save_register();
}

/* This backport is intended to be playable.  Keep imperfect flags until
   Xbox 360 hardware testing confirms every priority/blending/sound detail. */
GAMEX(1993, raiden2,  0, raiden2, raiden2, raiden2, ROT270, "Seibu Kaihatsu", "Raiden II (US, set 1)", GAME_IMPERFECT_GRAPHICS | GAME_IMPERFECT_SOUND)
GAMEX(1994, raidendx, 0, raiden2, raiden2, raiden2, ROT270, "Seibu Kaihatsu", "Raiden DX (UK)", GAME_NOT_WORKING )
