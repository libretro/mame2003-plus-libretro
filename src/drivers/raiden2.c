/***************************************************************************

    Seibu Raiden II / Raiden DX / Zero Team hardware (SEI1000 COP, SEI252,
    SEI360) and the later V33 boards (New Zero Team, Zero Team 2000,
    Raiden II New / Raiden DX)

    Backport of current MAME's raiden2.cpp / r2dx_v33.cpp to this core:
    - V30 program banking (Raiden II, Zero Team: 2 x 128 KB; Raiden DX:
      16 x 64 KB), V33 game select banking
    - COP commands, sprite protection, collision, sort DMA, as in MAME/FBNeo
    - tilemap and palette DMA into the private video memory, palette fades
    - sprite ROM decryption (SEI252 / Zero Team key)
    - MAME's compositing: layers and sprites as palette indexes, with the
      blended pens drawn at 50%
    - Seibu sound: YM2151 + 2 x OKI (Raiden II/DX), YM3812 + OKI (Zero Team)

    Builds on the earlier Raiden II backport by Salvia and arcadez2003.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/seibu.h"
#include "sound/2151intf.h"
#include "sound/adpcm.h"
#include "sound/3812intf.h"
#include "machine/eeprom.h"
#include "state.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* seibu_pending_w exists in sndhrdw/seibu.c but is not declared by the old header */
WRITE_HANDLER( seibu_pending_w );

static struct tilemap *r2play_background_layer,*r2play_midground_layer,*r2play_foreground_layer,*r2play_text_layer;
static int r2play_bg_bank, r2play_mid_bank, r2play_fg_bank, r2play_tx_bank;
static UINT8 r2_scroll[12];
static UINT16 r2_layer_enable;
static UINT16 r2_prg_bank;
static int r2_is_raidendx;
/* The old memory system uses REGION_CPU1[0..1ffff] as writable RAM.
   Preserve both switchable ROM pages before the first CPU/reset runs. */
static UINT8 *r2_bank_rom;

/* Board families sharing this driver */
#define R2_GAME_RAIDEN2		0
#define R2_GAME_RAIDENDX	1
#define R2_GAME_ZEROTEAM	2
#define R2_GAME_V33			3
static int r2_game;
static int r2_is_r2dx_v33;		/* Raiden II New / Raiden DX: game select banking */

/* Private video memory of the SEI252/SEI360 (or the V33 board), filled by the
   tilemap and palette DMA: the CPU-side RAM is only the DMA source. */
static UINT16 r2_back_data[0x800/2], r2_fore_data[0x800/2], r2_mid_data[0x800/2], r2_text_data[0x1000/2];
static UINT16 r2_palette_data[0x1000/2];

/* Pens drawn at 50% over what is below (shadows, clouds, engine flames, water) */
static UINT8 r2_blend_active[0x800];

static unsigned r2_video_enable(void)
{
   return r2_layer_enable;
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
static UINT16 r2_pal_brightness_val;
static UINT16 r2_cop_bank;
static UINT16 r2_sprite_prot_x, r2_sprite_prot_y, r2_dst1, r2_cop_spr_maxx, r2_cop_spr_off;
static UINT16 r2_sprite_prot_src_addr[2];
static UINT16 r2_cop_hit_status, r2_cop_hit_baseadr, r2_cop_hit_val_stat;
static INT16 r2_cop_hit_val[3];
static UINT32 r2_cop_sort_ram_addr, r2_cop_sort_lookup;
static UINT16 r2_cop_sort_param;
static UINT16 r2_pal_brightness_mode, r2_cop_rng_max;

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
	r2_pal_brightness_val = 0;
	r2_cop_bank = 0;
	r2_sprite_prot_x = r2_sprite_prot_y = r2_dst1 = r2_cop_spr_maxx = r2_cop_spr_off = 0;
	memset(r2_sprite_prot_src_addr, 0, sizeof(r2_sprite_prot_src_addr));
	memset(r2_collision, 0, sizeof(r2_collision));
	r2_cop_hit_status = r2_cop_hit_baseadr = r2_cop_hit_val_stat = 0;
	memset(r2_cop_hit_val, 0, sizeof(r2_cop_hit_val));
	r2_cop_sort_ram_addr = r2_cop_sort_lookup = 0;
	r2_cop_sort_param = 0;
	r2_pal_brightness_mode = 0;
	r2_cop_rng_max = 0;
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
		case 0x7e05: /* raidendx */
			cpu_writemem20(0x470, r2_mem_r8(r2_cop_regs[4]));
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

static void r2_tile_w(UINT32 offset, UINT16 data);
static void r2_palette_w(UINT32 offset, UINT16 data);

/* mode 0x14: the tilemaps, 0x2800 bytes, into the private buffers */
static void r2_dma_tilemap_buffer(void)
{
	UINT32 src = (UINT32)r2_cop_dma_src[r2_cop_dma_mode] << 6;
	UINT32 i;
	if (src == 0xcfc0) src = 0xd000; /* Raiden II, as in MAME */
	for (i = 0; i < 0x2800 / 2; i++)
	{
		r2_tile_w(i, r2_mem_r16(src));
		src += 2;
	}
}

/* mode 0x15: the palette, 0x1000 bytes, into the private buffer */
static void r2_dma_palette_buffer(void)
{
	UINT32 src = (UINT32)r2_cop_dma_src[r2_cop_dma_mode] << 6;
	UINT32 i;
	for (i = 0; i < 0x1000 / 2; i++)
	{
		r2_palette_w(i, r2_mem_r16(src));
		src += 2;
	}
}

/* modes 0x80-0x87: palette fade, from one RAM buffer to another */
static void r2_dma_palette_brightness(void)
{
	UINT32 src = (UINT32)r2_cop_dma_src[r2_cop_dma_mode] << 6;
	UINT32 dst = (UINT32)r2_cop_dma_dst[r2_cop_dma_mode] << 6;
	UINT32 size = (((UINT32)r2_cop_dma_size[r2_cop_dma_mode] << 5) - ((UINT32)r2_cop_dma_dst[r2_cop_dma_mode] << 6) + 0x20) / 2;
	UINT32 i;

	for (i = 0; i < size; i++)
	{
		UINT16 pal_val, paldata, target;
		int r, g, b, rt, gt, bt;

		paldata = r2_mem_r16(src);
		target = r2_mem_r16(src + r2_cop_dma_adr_rel * 0x400);
		if (r2_pal_brightness_mode == 5)
		{
			if (paldata & 0x8000)
				pal_val = paldata; /* not faded */
			else
			{
				bt = r2_fade_table(((target & 0x7c00) >> 5) | (r2_pal_brightness_val ^ 0));
				b = r2_fade_table(((paldata & 0x7c00) >> 5) | (r2_pal_brightness_val ^ 0x1f));
				pal_val = (UINT16)(((b + bt) & 0x1f) << 10);
				gt = r2_fade_table((target & 0x03e0) | (r2_pal_brightness_val ^ 0));
				g = r2_fade_table((paldata & 0x03e0) | (r2_pal_brightness_val ^ 0x1f));
				pal_val |= (UINT16)(((g + gt) & 0x1f) << 5);
				rt = r2_fade_table(((target & 0x001f) << 5) | (r2_pal_brightness_val ^ 0));
				r = r2_fade_table(((paldata & 0x001f) << 5) | (r2_pal_brightness_val ^ 0x1f));
				pal_val |= (UINT16)((r + rt) & 0x1f);
			}
		}
		else if (r2_pal_brightness_mode == 4)
		{
			bt = (target & 0x7c00) >> 10; b = (paldata & 0x7c00) >> 10;
			gt = (target & 0x03e0) >> 5;  g = (paldata & 0x03e0) >> 5;
			rt = target & 0x001f;         r = paldata & 0x001f;
			if (r2_pal_brightness_val == 0x10 || r2_pal_brightness_val == 0xffff)
				pal_val = (UINT16)(bt << 10 | gt << 5 | rt);
			else
			{
				int v = r2_pal_brightness_val * 2;
				bt = r2_fade_table(bt << 5 | (v ^ 0));
				b = r2_fade_table(b << 5 | (v ^ 0x1f));
				pal_val = (UINT16)(((b + bt) & 0x1f) << 10);
				gt = r2_fade_table(gt << 5 | (v ^ 0));
				g = r2_fade_table(g << 5 | (v ^ 0x1f));
				pal_val |= (UINT16)(((g + gt) & 0x1f) << 5);
				rt = r2_fade_table(rt << 5 | (v ^ 0));
				r = r2_fade_table(r << 5 | (v ^ 0x1f));
				pal_val |= (UINT16)((r + rt) & 0x1f);
			}
		}
		else
			pal_val = paldata;

		r2_mem_w16(dst, pal_val);
		src += 2;
		dst += 2;
	}
}

static void r2_cop_dma_trigger(void)
{
	UINT32 src, dst, size, i, address, length;
	UINT16 mode = r2_cop_dma_mode & 0x1ff;

	switch (mode)
	{
		case 0x14:
			r2_dma_tilemap_buffer();
			break;
		case 0x15:
			r2_dma_palette_buffer();
			break;
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
			r2_dma_palette_brightness();
			break;
		case 0x09: case 0x0e:
			src = (UINT32)r2_cop_dma_src[mode] << 6;
			dst = (UINT32)r2_cop_dma_dst[mode] << 6;
			size = (((UINT32)r2_cop_dma_size[mode] << 5) - ((UINT32)r2_cop_dma_dst[mode] << 6) + 0x20) / 2;
			for (i = 0; i < size; ++i) { r2_mem_w16(dst, r2_mem_r16(src)); src += 2; dst += 2; }
			break;
		case 0x118: case 0x119: case 0x11a: case 0x11b:
		case 0x11c: case 0x11d: case 0x11e: case 0x11f:
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
	if (r2_is_raidendx)
		o = (o & ~0x30) | ((o & 0x10) << 1) | ((o & 0x20) >> 1);
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
	if (rom)
	{
		cpu_setbank(3, rom + bb * 0x20000);
		cpu_setbank(4, rom + bb * 0x20000 + 0x10000);
	}
}

static int r2_dx_bank;

static void r2_dx_bankswitch(int bb)
{
	UINT8 *cpu1 = memory_region(REGION_CPU1);
	r2_dx_bank = bb & 0x0f;
	if (cpu1 && memory_region_length(REGION_CPU1) >= 0x200000)
	{
		cpu_setbank(3, cpu1 + 0x100000 + (bb & 0x0f) * 0x10000);
		cpu_setbank(4, cpu1 + 0x30000);
	}
}

static READ_HANDLER(r2_cop_r)
{
	UINT32 addr = R2_COP_BASE + offset;
	UINT16 ret;
	UINT32 pair = addr & 0x7fe;

	if (r2_is_raidendx && addr >= 0x600 && addr <= 0x64f)
	{
		UINT32 o = addr & 0x7e;
		o = (o & ~0x30) | ((o & 0x10) << 1) | ((o & 0x20) >> 1);
		pair = (addr & ~0x7e) | o;
	}

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
		case 0x5a0: case 0x5a2: case 0x5a4: case 0x5a6:
			/* random numbers: the CPU cycle count, as in MAME */
			ret = (UINT16)(activecpu_gettotalcycles() % ((UINT32)r2_cop_rng_max + 1));
			break;
		case 0x5b0: ret = r2_cop_status; break;
		case 0x5b2: ret = r2_cop_dist; break;
		case 0x5b4: ret = r2_cop_angle; break;
		case 0x6c0: ret = r2_cop_spr_off; break;
		case 0x6c2: ret = r2_sprite_prot_src_addr[0]; break;
		case 0x6dc: ret = r2_cop_spr_maxx; break;
		default: break;
	}
	return (ret >> ((addr & 1) * 8)) & 0xff;
}

static WRITE_HANDLER(r2_cop_w)
{
	UINT32 addr = R2_COP_BASE + offset;
	UINT32 o = addr - R2_COP_BASE;
	UINT32 pair = addr & 0x7fe;
	UINT16 w;
	int idx;

	if (r2_is_raidendx && addr >= 0x600 && addr <= 0x64f)
	{
		UINT32 crtc_o = addr & 0x7f;
		crtc_o = (crtc_o & ~0x30) | ((crtc_o & 0x10) << 1) | ((crtc_o & 0x20) >> 1);
		o = (addr & ~0x7f) - R2_COP_BASE + crtc_o;
	}

	if (o < sizeof(r2_copram)) r2_copram[o] = data;
	if (addr >= 0x600 && addr <= 0x64f) r2_crtc_write(addr, (UINT8)data);

	/* tile bank 0/1: written as a byte */
	if (addr == 0x6cc && r2_game != R2_GAME_V33)
	{
		int nb = (data & 1) << 1;
		int nm = (data & 2) | 1;
		if (r2play_bg_bank != nb) { r2play_bg_bank = nb; if (r2play_background_layer) tilemap_mark_all_tiles_dirty(r2play_background_layer); }
		if (r2play_mid_bank != nm) { r2play_mid_bank = nm; if (r2play_midground_layer) tilemap_mark_all_tiles_dirty(r2play_midground_layer); }
	}

	/* no tilemap bank register on Zero Team */
	if ((addr == 0x470 || addr == 0x471) && r2_game != R2_GAME_ZEROTEAM && r2_game != R2_GAME_V33)
	{
		r2_cop_bank = r2_copram_word(0x470);
		if (r2_is_raidendx)
		{
			int new_bank = 4 | ((r2_cop_bank >> 4) & 3);
			if (r2play_fg_bank != new_bank)
			{
				r2play_fg_bank = new_bank;
				if (r2play_foreground_layer) tilemap_mark_all_tiles_dirty(r2play_foreground_layer);
			}
			r2_dx_bankswitch((r2_cop_bank >> 12) & 0x0f);
		}
		else
		{
			int new_bank = 4 | (r2_cop_bank >> 14);
			if (r2play_fg_bank != new_bank)
			{
				r2play_fg_bank = new_bank;
				if (r2play_foreground_layer) tilemap_mark_all_tiles_dirty(r2play_foreground_layer);
			}
		}
	}

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
		case 0x42c: r2_cop_rng_max = w; break;
		case 0x436: r2_cop_hit_baseadr = w; break;
		case 0x444: r2_cop_scale = w & 3; break;
		case 0x450: r2_cop_sort_ram_addr = (r2_cop_sort_ram_addr & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x452: r2_cop_sort_ram_addr = (r2_cop_sort_ram_addr & 0xffff0000U) | w; break;
		case 0x454: r2_cop_sort_lookup = (r2_cop_sort_lookup & 0x0000ffffU) | ((UINT32)w << 16); break;
		case 0x456: r2_cop_sort_lookup = (r2_cop_sort_lookup & 0xffff0000U) | w; break;
		case 0x458: r2_cop_sort_param = w; break;
		case 0x45a: r2_pal_brightness_val = w; break;
		case 0x45c: r2_pal_brightness_mode = w; break;
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
		case 0x6ca:
			/* program bank, bit 7 of 0x6cb; on Raiden DX it selects entry 0 or 1
			   of the same 16-entry bank as 0x470 */
			if (r2_game == R2_GAME_RAIDENDX)
				r2_dx_bankswitch((~w >> 15) & 1);
			else if (r2_game != R2_GAME_V33)
				r2_main_bankswitch(w);
			break;
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

/* Carry propagation terminates at every zero bit of carry_mask.  This
 * parallel form is exactly the bit-serial operation above, including its
 * final bit-31 wrap XOR. The fixed Raiden II mask has only short carry runs.
 */
static UINT32 r2_partial_carry_fast(UINT32 a, UINT32 b, UINT32 mask)
{
	UINT32 result = a ^ b;
	UINT32 carry = a & b;
	UINT32 wrap = 0;
	while (carry & mask)
	{
		carry &= mask;
		wrap ^= carry >> 31;
		carry <<= 1;
		b = result & carry;
		result ^= carry;
		carry = b;
	}
	return result ^ wrap;
}

static int r2play_prepare_and_decrypt_sprites(UINT8 *rom)
{
	/* Five KiB of load-time stack tables replaces per-word bit-walking.
	 * No persistent cache and no extra copy of the 8 MiB sprite region. */
	UINT32 swap32[4][256];
	UINT16 swap16[2][256];
	UINT16 gm[16];
	UINT32 i, byte, lane;
	for (lane = 0; lane < 4; ++lane)
		for (byte = 0; byte < 256; ++byte)
			swap32[lane][byte] = r2_bitswap32(byte << (lane * 8));
	for (lane = 0; lane < 2; ++lane)
		for (byte = 0; byte < 256; ++byte)
			swap16[lane][byte] = r2_bitswap16((UINT16)(byte << (lane * 8)));
	for (i = 0; i < 16; ++i) gm[i] = r2_gm(i);
	for (i = 0; i < 0x800000U / 4U; ++i)
	{
		UINT32 i2 = (i & 0xff) ^ ((i >> 15) & 1);
		UINT32 i1 = i2 ^ (((i >> 20) & 1) << 8);
		UINT32 v = r2_yrot(r2_read32le_buf(rom + i * 4), r2_rotate[i1]);
		UINT16 low = (UINT16)((r2_x5[i2] << 11) ^ r2_x11[(i >> 8) & 0xff] ^ gm[(i >> 16) & 15]);
		UINT16 high = (UINT16)(swap16[0][low & 255] | swap16[1][low >> 8]);
		UINT32 key = ((UINT32)low | ((UINT32)high << 16)) ^ 0x60860000U;
		v = swap32[0][v & 255] | swap32[1][(v >> 8) & 255] |
		    swap32[2][(v >> 16) & 255] | swap32[3][v >> 24];
		r2_write32le_buf(rom + i * 4, r2_partial_carry_fast(v, key, 0x176c91a8U) ^ 0x0f488000U);
	}
	return 0;
}

static const UINT8 rotate_zt[256] = {
	0x0a, 0x0a, 0x0b, 0x0e, 0x02, 0x12, 0x0d, 0x16, 0x01, 0x1b, 0x08, 0x15, 0x03, 0x14, 0x0f, 0x1f,
	0x10, 0x1d, 0x03, 0x0b, 0x16, 0x07, 0x10, 0x19, 0x1f, 0x1c, 0x03, 0x0b, 0x15, 0x0d, 0x00, 0x14,
	0x15, 0x0d, 0x0c, 0x1b, 0x18, 0x18, 0x0b, 0x1b, 0x16, 0x17, 0x01, 0x0e, 0x14, 0x0c, 0x1e, 0x13,
	0x13, 0x12, 0x19, 0x0b, 0x1b, 0x1a, 0x0d, 0x04, 0x00, 0x00, 0x12, 0x06, 0x0f, 0x1c, 0x19, 0x1b,
	0x01, 0x04, 0x15, 0x1c, 0x02, 0x13, 0x0f, 0x09, 0x1c, 0x05, 0x09, 0x05, 0x0d, 0x17, 0x13, 0x1f,
	0x17, 0x19, 0x0f, 0x04, 0x05, 0x03, 0x0c, 0x03, 0x13, 0x02, 0x09, 0x0d, 0x1d, 0x1a, 0x02, 0x0d,
	0x0d, 0x0f, 0x13, 0x1c, 0x05, 0x03, 0x09, 0x08, 0x03, 0x1d, 0x05, 0x1f, 0x0b, 0x15, 0x06, 0x10,
	0x0c, 0x15, 0x18, 0x0c, 0x18, 0x1f, 0x1d, 0x1d, 0x1d, 0x0e, 0x1f, 0x0c, 0x14, 0x0d, 0x0a, 0x08,
	0x0f, 0x12, 0x08, 0x06, 0x0d, 0x1d, 0x19, 0x1e, 0x1c, 0x08, 0x09, 0x1e, 0x1a, 0x14, 0x08, 0x17,
	0x11, 0x0e, 0x1a, 0x18, 0x01, 0x08, 0x05, 0x01, 0x06, 0x1e, 0x15, 0x1d, 0x0c, 0x09, 0x1b, 0x07,
	0x12, 0x00, 0x0c, 0x05, 0x03, 0x08, 0x19, 0x09, 0x09, 0x06, 0x13, 0x12, 0x0b, 0x1a, 0x09, 0x09,
	0x03, 0x09, 0x00, 0x07, 0x1c, 0x10, 0x14, 0x01, 0x17, 0x03, 0x05, 0x11, 0x1e, 0x12, 0x14, 0x19,
	0x10, 0x10, 0x08, 0x08, 0x07, 0x0c, 0x12, 0x1f, 0x02, 0x08, 0x1f, 0x14, 0x1e, 0x14, 0x02, 0x13,
	0x04, 0x10, 0x1e, 0x08, 0x0b, 0x14, 0x19, 0x1a, 0x07, 0x1c, 0x13, 0x07, 0x10, 0x14, 0x15, 0x1e,
	0x18, 0x0a, 0x0c, 0x1f, 0x0e, 0x05, 0x11, 0x08, 0x04, 0x06, 0x12, 0x0c, 0x00, 0x04, 0x0b, 0x07,
	0x14, 0x0a, 0x0c, 0x16, 0x02, 0x0a, 0x0f, 0x0d, 0x0c, 0x1d, 0x1e, 0x13, 0x0a, 0x08, 0x12, 0x0b
};

static const UINT8 x5_zt[256] = {
	0x0d, 0x14, 0x13, 0x1b, 0x1a, 0x05, 0x1a, 0x05, 0x1b, 0x17, 0x0d, 0x0f, 0x00, 0x17, 0x09, 0x04,
	0x13, 0x0f, 0x14, 0x17, 0x14, 0x0f, 0x1b, 0x0a, 0x0a, 0x15, 0x07, 0x0b, 0x0e, 0x05, 0x18, 0x12,
	0x1c, 0x01, 0x17, 0x02, 0x04, 0x00, 0x06, 0x12, 0x00, 0x0a, 0x1c, 0x19, 0x10, 0x1b, 0x16, 0x03,
	0x1c, 0x13, 0x16, 0x13, 0x15, 0x1d, 0x04, 0x14, 0x17, 0x1d, 0x01, 0x03, 0x1f, 0x1f, 0x1a, 0x10,
	0x08, 0x0c, 0x09, 0x16, 0x04, 0x19, 0x03, 0x14, 0x0c, 0x1d, 0x09, 0x08, 0x00, 0x1c, 0x0b, 0x18,
	0x1b, 0x00, 0x16, 0x0f, 0x0b, 0x1c, 0x0d, 0x16, 0x09, 0x15, 0x0c, 0x01, 0x18, 0x04, 0x04, 0x03,
	0x11, 0x0f, 0x03, 0x17, 0x04, 0x0a, 0x05, 0x1b, 0x08, 0x07, 0x0e, 0x08, 0x06, 0x16, 0x0f, 0x0d,
	0x0a, 0x14, 0x0a, 0x11, 0x14, 0x02, 0x08, 0x1f, 0x0e, 0x13, 0x0e, 0x02, 0x08, 0x13, 0x04, 0x19,
	0x0e, 0x00, 0x01, 0x1d, 0x02, 0x19, 0x08, 0x1c, 0x13, 0x09, 0x11, 0x14, 0x1e, 0x04, 0x03, 0x18,
	0x1f, 0x05, 0x0f, 0x03, 0x09, 0x0d, 0x12, 0x0f, 0x0d, 0x0e, 0x15, 0x1a, 0x17, 0x1c, 0x1d, 0x10,
	0x15, 0x00, 0x18, 0x18, 0x00, 0x08, 0x08, 0x01, 0x0d, 0x10, 0x14, 0x12, 0x03, 0x1c, 0x05, 0x0a,
	0x0f, 0x0d, 0x15, 0x00, 0x0b, 0x1f, 0x00, 0x1d, 0x1d, 0x1a, 0x13, 0x16, 0x08, 0x1b, 0x03, 0x12,
	0x02, 0x10, 0x0e, 0x10, 0x04, 0x05, 0x09, 0x07, 0x1d, 0x04, 0x05, 0x06, 0x17, 0x05, 0x1a, 0x04,
	0x1f, 0x0e, 0x09, 0x13, 0x15, 0x12, 0x1c, 0x13, 0x03, 0x09, 0x12, 0x08, 0x19, 0x17, 0x00, 0x19,
	0x1f, 0x06, 0x1c, 0x05, 0x18, 0x0a, 0x1f, 0x1e, 0x0d, 0x05, 0x1e, 0x1a, 0x11, 0x13, 0x15, 0x0c,
	0x05, 0x16, 0x11, 0x06, 0x12, 0x0c, 0x0c, 0x0f, 0x1c, 0x18, 0x08, 0x1e, 0x1c, 0x16, 0x15, 0x03
};

static const UINT16 x11_zt[512] = {
	0x7e9, 0x625, 0x625, 0x006, 0x006, 0x14a, 0x14a, 0x7e9, 0x657, 0x417, 0x417, 0x1a7, 0x1a7, 0x22f, 0x22f, 0x657,
	0x37e, 0x447, 0x447, 0x117, 0x117, 0x735, 0x735, 0x37e, 0x3be, 0x487, 0x487, 0x724, 0x724, 0x6e3, 0x6e3, 0x3be,
	0x646, 0x035, 0x035, 0x57e, 0x57e, 0x134, 0x134, 0x646, 0x09a, 0x4d1, 0x4d1, 0x511, 0x511, 0x5d5, 0x5d5, 0x09a,
	0x3e2, 0x5c6, 0x5c6, 0x3b6, 0x3b6, 0x354, 0x354, 0x3e2, 0x6e3, 0x7d6, 0x7d6, 0x317, 0x317, 0x1f0, 0x1f0, 0x6e3,
	0x115, 0x462, 0x462, 0x620, 0x620, 0x0ce, 0x0ce, 0x115, 0x715, 0x7ec, 0x7ec, 0x1e6, 0x1e6, 0x610, 0x610, 0x715,
	0x34a, 0x718, 0x718, 0x7ba, 0x7ba, 0x288, 0x288, 0x34a, 0x6e7, 0x193, 0x193, 0x3d2, 0x3d2, 0x78f, 0x78f, 0x6e7,
	0x57c, 0x014, 0x014, 0x6b3, 0x6b3, 0x204, 0x204, 0x57c, 0x3f4, 0x782, 0x782, 0x5b4, 0x5b4, 0x375, 0x375, 0x3f4,
	0x17d, 0x660, 0x660, 0x48d, 0x48d, 0x7ff, 0x7ff, 0x17d, 0x5b1, 0x39d, 0x39d, 0x48c, 0x48c, 0x5ab, 0x5ab, 0x5b1,
	0x3d3, 0x4f6, 0x4f6, 0x7b9, 0x7b9, 0x5df, 0x5df, 0x3d3, 0x349, 0x4c5, 0x4c5, 0x403, 0x403, 0x0ab, 0x0ab, 0x349,
	0x46b, 0x790, 0x790, 0x59f, 0x59f, 0x2b1, 0x2b1, 0x46b, 0x6c5, 0x4e7, 0x4e7, 0x023, 0x023, 0x0b0, 0x0b0, 0x6c5,
	0x2d6, 0x3e2, 0x3e2, 0x7f3, 0x7f3, 0x376, 0x376, 0x2d6, 0x2d7, 0x0bf, 0x0bf, 0x14b, 0x14b, 0x65b, 0x65b, 0x2d7,
	0x68d, 0x360, 0x360, 0x434, 0x434, 0x5fa, 0x5fa, 0x68d, 0x654, 0x386, 0x386, 0x28b, 0x28b, 0x2e1, 0x2e1, 0x654,
	0x1ee, 0x4e8, 0x4e8, 0x482, 0x482, 0x471, 0x471, 0x1ee, 0x221, 0x3de, 0x3de, 0x467, 0x467, 0x21b, 0x21b, 0x221,
	0x7a6, 0x6b6, 0x6b6, 0x157, 0x157, 0x658, 0x658, 0x7a6, 0x1e9, 0x6e3, 0x6e3, 0x340, 0x340, 0x2cd, 0x2cd, 0x1e9,
	0x045, 0x566, 0x566, 0x1f0, 0x1f0, 0x5a1, 0x5a1, 0x045, 0x559, 0x0d4, 0x0d4, 0x67c, 0x67c, 0x21a, 0x21a, 0x559,
	0x187, 0x13b, 0x13b, 0x68e, 0x68e, 0x0f6, 0x0f6, 0x187, 0x2b5, 0x4c2, 0x4c2, 0x2b6, 0x2b6, 0x6a4, 0x6a4, 0x2b5,
	0x56b, 0x1b2, 0x1b2, 0x15d, 0x15d, 0x3aa, 0x3aa, 0x56b, 0x506, 0x67a, 0x67a, 0x5db, 0x5db, 0x5ef, 0x5ef, 0x506,
	0x5ab, 0x3ed, 0x3ed, 0x2b3, 0x2b3, 0x504, 0x504, 0x5ab, 0x137, 0x761, 0x761, 0x735, 0x735, 0x6e9, 0x6e9, 0x137,
	0x35a, 0x7bb, 0x7bb, 0x190, 0x190, 0x0a8, 0x0a8, 0x35a, 0x797, 0x164, 0x164, 0x1b8, 0x1b8, 0x761, 0x761, 0x797,
	0x138, 0x076, 0x076, 0x613, 0x613, 0x055, 0x055, 0x138, 0x2e2, 0x493, 0x493, 0x0c2, 0x0c2, 0x785, 0x785, 0x2e2,
	0x60b, 0x41a, 0x41a, 0x22a, 0x22a, 0x2a1, 0x2a1, 0x60b, 0x5cb, 0x092, 0x092, 0x656, 0x656, 0x5db, 0x5db, 0x5cb,
	0x00a, 0x490, 0x490, 0x39d, 0x39d, 0x572, 0x572, 0x00a, 0x600, 0x4cd, 0x4cd, 0x33f, 0x33f, 0x0bc, 0x0bc, 0x600,
	0x047, 0x685, 0x685, 0x4b0, 0x4b0, 0x330, 0x330, 0x047, 0x7b4, 0x590, 0x590, 0x1db, 0x1db, 0x349, 0x349, 0x7b4,
	0x6ae, 0x7a3, 0x7a3, 0x6fa, 0x6fa, 0x2d4, 0x2d4, 0x6ae, 0x117, 0x2fe, 0x2fe, 0x756, 0x756, 0x627, 0x627, 0x117,
	0x337, 0x55e, 0x55e, 0x2ef, 0x2ef, 0x2a9, 0x2a9, 0x337, 0x382, 0x02a, 0x02a, 0x4af, 0x4af, 0x7fa, 0x7fa, 0x382,
	0x73f, 0x6d8, 0x6d8, 0x7d2, 0x7d2, 0x52d, 0x52d, 0x73f, 0x385, 0x303, 0x303, 0x3eb, 0x3eb, 0x689, 0x689, 0x385,
	0x6cd, 0x4c3, 0x4c3, 0x2df, 0x2df, 0x1d5, 0x1d5, 0x6cd, 0x7da, 0x44c, 0x44c, 0x764, 0x764, 0x7f6, 0x7f6, 0x7da,
	0x7c1, 0x276, 0x276, 0x27f, 0x27f, 0x1c9, 0x1c9, 0x7c1, 0x58c, 0x001, 0x001, 0x07f, 0x07f, 0x62b, 0x62b, 0x58c,
	0x374, 0x746, 0x746, 0x5b2, 0x5b2, 0x35e, 0x35e, 0x374, 0x64d, 0x1fd, 0x1fd, 0x2dc, 0x2dc, 0x550, 0x550, 0x64d,
	0x33d, 0x40c, 0x40c, 0x3a4, 0x3a4, 0x293, 0x293, 0x33d, 0x0c0, 0x462, 0x462, 0x6fb, 0x6fb, 0x4b0, 0x4b0, 0x0c0,
	0x2aa, 0x175, 0x175, 0x01e, 0x01e, 0x762, 0x762, 0x2aa, 0x216, 0x76b, 0x76b, 0x728, 0x728, 0x15f, 0x15f, 0x216,
	0x30e, 0x0d8, 0x0d8, 0x55c, 0x55c, 0x31c, 0x31c, 0x30e, 0x351, 0x3dc, 0x3dc, 0x6c1, 0x6c1, 0x651, 0x651, 0x351
};

static int zeroteam_prepare_and_decrypt_sprites(UINT8 *rom)
{
	UINT32 swap32[4][256];
	UINT16 swap16[2][256];
	UINT16 gm[16];
	UINT32 i, byte, lane;
	for (lane = 0; lane < 4; ++lane)
		for (byte = 0; byte < 256; ++byte)
			swap32[lane][byte] = r2_bitswap32(byte << (lane * 8));
	for (lane = 0; lane < 2; ++lane)
		for (byte = 0; byte < 256; ++byte)
			swap16[lane][byte] = r2_bitswap16((UINT16)(byte << (lane * 8)));
	for (i = 0; i < 16; ++i) gm[i] = r2_gm(i);
	for (i = 0; i < 0x400000U / 4U; ++i)
	{
		UINT32 i1 = i & 0xff;
		UINT32 i2 = i & 0xff;
		UINT32 i3 = (i >> 7) & 0x1ff;
		UINT32 v = r2_yrot(r2_read32le_buf(rom + i * 4), rotate_zt[i1]);
		UINT16 low = (UINT16)((x5_zt[i2] << 11) ^ x11_zt[i3] ^ gm[(i >> 16) & 15]);
		UINT16 high = (UINT16)(swap16[0][low & 255] | swap16[1][low >> 8]);
		UINT32 key = ((UINT32)low | ((UINT32)high << 16)) ^ 0xa5800000U;
		v = swap32[0][v & 255] | swap32[1][(v >> 8) & 255] |
		    swap32[2][(v >> 16) & 255] | swap32[3][v >> 24];
		r2_write32le_buf(rom + i * 4, r2_partial_carry_fast(v, key, 0x7b67b7b9U) ^ 0xf1412ea8U);
	}
	return 0;
}

/* -----------------------------------------------------------------------
 * Video
 * ----------------------------------------------------------------------- */

/* Video

   As in current MAME: the layers and the sprites are drawn as palette indexes
   into private bitmaps, then mixed into the screen bitmap, where the pens
   listed in the blend table are drawn at 50% over what is below. The screen
   bitmap is direct RGB, so the mixing reads the colours from Machine->pens.
   All the bitmaps share the screen geometry and are drawn by the core
   (tilemap_draw, drawgfx), so the core's own rotation still applies. */

static const UINT16 raiden_blended_colors[] =
{
	0x380,	/* bridge tunnel entrance shadow */
	/* cloud */
	0x3c0, 0x3c1, 0x3c2, 0x3c3, 0x3c4, 0x3c5, 0x3c6, 0x3c7, 0x3c8, 0x3c9, 0x3ca, 0x3cb, 0x3cc, 0x3cd, 0x3ce,
	/* engine */
	0x3d0, 0x3d1, 0x3d2, 0x3d3, 0x3d4, 0x3d5, 0x3d6, 0x3d7, 0x3d8, 0x3d9, 0x3da, 0x3db, 0x3dc, 0x3dd, 0x3de,
	/* level 1 boss legs */
	0x3f0, 0x3f1, 0x3f2, 0x3f3, 0x3f4, 0x3f5, 0x3f6, 0x3f7, 0x3f8, 0x3f9, 0x3fa, 0x3fb, 0x3fc, 0x3fd, 0x3fe,
	/* water */
	0x4f8, 0x4f9, 0x4fa, 0x4fb, 0x4fc, 0x4fd, 0x4fe,
	0x5c8, 0x5c9, 0x5ca, 0x5cb, 0x5cc, 0x5cd, 0x5ce,
	0x5de,	/* wall shadow */
	/* glass roof */
	0x5e8, 0x5e9, 0x5ea, 0x5eb, 0x5ec, 0x5ed, 0x5ee,
	/* house shadow plus stage 3 boss green pools */
	0x5f8, 0x5f9, 0x5fa, 0x5fb, 0x5fc, 0x5fd, 0x5fe,
	/* water and trees */
	0x6c8, 0x6c9, 0x6ca, 0x6cb, 0x6cc, 0x6cd, 0x6ce,
	0x6d8, 0x6d9, 0x6da, 0x6db, 0x6dc, 0x6dd, 0x6de,
	0x6e8, 0x6e9, 0x6ea, 0x6eb, 0x6ec, 0x6ed, 0x6ee,
	0x6f8, 0x6f9, 0x6fa, 0x6fb, 0x6fc, 0x6fd, 0x6fe,
	/* stage end panel, raiden dx logo plus misc stuff */
	0x70d, 0x70e,
	0x71c, 0x71d, 0x71e,
	0x72d, 0x72e,
	0x73d, 0x73e,
	0x74d, 0x74e,
	0x75c,
	0x76c, 0x76d, 0x76e,
	0x77d, 0x77e,
	/* logo in attract mode */
	0x7c8, 0x7c9, 0x7ca, 0x7cb, 0x7cc, 0x7cd, 0x7ce,
	0xffff
};

static const UINT16 zeroteam_blended_colors[] =
{
	0x37e,	/* player selection */
	0x38e,	/* boss spear shadow */
	0x52e,	/* scaffolding shadow */
	0x5de,	/* road brightening */
	0xffff
};

static void r2_init_blending(const UINT16 *table)
{
	memset(r2_blend_active, 0, sizeof(r2_blend_active));
	while (*table != 0xffff)
		r2_blend_active[*table++] = 1;
}

static struct mame_bitmap *r2_tile_bitmap, *r2_sprite_bitmap;
static pen_t r2_identity_pens[0x800];			/* palette index -> palette index */
static pen_t r2_sprite_pens[4][0x400];			/* sprite pen -> index | priority << 14 */
static struct GfxElement r2_sprite_gfx[4];

static void r2_tile_w(UINT32 offset, UINT16 data)
{
	if (offset < 0x800 / 2)
	{
		if (r2_back_data[offset] != data) { r2_back_data[offset] = data; tilemap_mark_tile_dirty(r2play_background_layer, offset); }
	}
	else if (offset < 0x1000 / 2)
	{
		offset -= 0x800 / 2;
		if (r2_fore_data[offset] != data) { r2_fore_data[offset] = data; tilemap_mark_tile_dirty(r2play_foreground_layer, offset); }
	}
	else if (offset < 0x1800 / 2)
	{
		offset -= 0x1000 / 2;
		if (r2_mid_data[offset] != data) { r2_mid_data[offset] = data; tilemap_mark_tile_dirty(r2play_midground_layer, offset); }
	}
	else if (offset < 0x2800 / 2)
	{
		offset -= 0x1800 / 2;
		if (r2_text_data[offset] != data) { r2_text_data[offset] = data; tilemap_mark_tile_dirty(r2play_text_layer, offset); }
	}
}

/* xBBBBBGGGGGRRRRR */
static void r2_palette_w(UINT32 offset, UINT16 data)
{
	int r, g, b;
	if (offset >= 0x1000 / 2) return;
	r2_palette_data[offset] = data;
	r = data & 0x1f; g = (data >> 5) & 0x1f; b = (data >> 10) & 0x1f;
	palette_set_color(offset, (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
}

static void r2play_get_back_tile_info(int tile_index)
{
	int v = r2_back_data[tile_index];
	SET_TILE_INFO(1, (v & 0x0fff) | (r2play_bg_bank << 12), (v >> 12) & 0x0f, 0)
}

static void r2play_get_mid_tile_info(int tile_index)
{
	int v = r2_mid_data[tile_index];
	SET_TILE_INFO(1, (v & 0x0fff) | (r2play_mid_bank << 12), ((v >> 12) & 0x0f) | 0x20, 0)
}

static void r2play_get_fore_tile_info(int tile_index)
{
	int v = r2_fore_data[tile_index];
	SET_TILE_INFO(1, (v & 0x0fff) | (r2play_fg_bank << 12), ((v >> 12) & 0x0f) | 0x10, 0)
}

static void r2play_get_text_tile_info(int tile_index)
{
	int v = r2_text_data[tile_index];
	SET_TILE_INFO(0, (v & 0x0fff) | (r2play_tx_bank << 12), (v >> 12) & 0x0f, 0)
}

static void r2_state_register(void);

VIDEO_START(r2play)
{
	int i, p;

	memset(r2_back_data, 0, sizeof(r2_back_data));
	memset(r2_fore_data, 0, sizeof(r2_fore_data));
	memset(r2_mid_data, 0, sizeof(r2_mid_data));
	memset(r2_text_data, 0, sizeof(r2_text_data));
	memset(r2_palette_data, 0, sizeof(r2_palette_data));
	if (buffered_spriteram)
		memset(buffered_spriteram, 0, spriteram_size);

	r2play_text_layer       = tilemap_create(r2play_get_text_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 8, 8, 64, 32);
	r2play_background_layer = tilemap_create(r2play_get_back_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	r2play_midground_layer  = tilemap_create(r2play_get_mid_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	r2play_foreground_layer = tilemap_create(r2play_get_fore_tile_info, tilemap_scan_rows, TILEMAP_TRANSPARENT, 16, 16, 32, 32);
	if (!r2play_text_layer || !r2play_background_layer || !r2play_midground_layer || !r2play_foreground_layer) return 1;
	tilemap_set_transparent_pen(r2play_background_layer, 15);
	tilemap_set_transparent_pen(r2play_midground_layer, 15);
	tilemap_set_transparent_pen(r2play_foreground_layer, 15);
	tilemap_set_transparent_pen(r2play_text_layer, 15);

	r2_tile_bitmap = auto_bitmap_alloc_depth(Machine->scrbitmap->width, Machine->scrbitmap->height, 16);
	r2_sprite_bitmap = auto_bitmap_alloc_depth(Machine->scrbitmap->width, Machine->scrbitmap->height, 16);
	if (!r2_tile_bitmap || !r2_sprite_bitmap) return 1;

	/* sprites: a copy of the sprite gfx per priority, whose colortable gives
	   the palette index with the priority in bits 14-15 */
	for (i = 0; i < 0x800; i++)
		r2_identity_pens[i] = i;
	for (p = 0; p < 4; p++)
	{
		for (i = 0; i < 0x400; i++)
			r2_sprite_pens[p][i] = i | (p << 14);
		r2_sprite_gfx[p] = *Machine->gfx[2];
		r2_sprite_gfx[p].colortable = r2_sprite_pens[p];
	}
	r2_state_register();
	return 0;
}

static const int r2_default_spri[5] = { 0, 1, 2, 3, -1 };
static const int r2_zeroteam_spri[5] = { -1, 0, 1, 2, 3 };
static const int *r2_cur_spri = r2_default_spri;

/* SEI25x sprites, last entry first, as palette index | priority << 14 */
static void r2_draw_sprites_raw(const struct rectangle *cliprect)
{
	const UINT8 *ram = buffered_spriteram;
	int count = spriteram_size / 8;
	int i;

	fillbitmap(r2_sprite_bitmap, 0xffff, cliprect);
	if (!ram) return;

	for (i = count - 1; i >= 0; i--)
	{
		const UINT8 *e = ram + i * 8;
		UINT16 attr = e[0] | (e[1] << 8);
		UINT32 code = e[2] | (e[3] << 8);
		int flipy = (attr >> 15) & 1;
		int sizey = ((attr >> 12) & 7) + 1;
		int flipx = (attr >> 11) & 1;
		int sizex = ((attr >> 8) & 7) + 1;
		int pri = (attr >> 6) & 3;
		int color = attr & 0x3f;
		int x = (e[4] | (e[5] << 8)) & 0x1ff;
		int y = (e[6] | (e[7] << 8)) & 0x1ff;
		int ax, ay;

		if ((code % r2_sprite_gfx[pri].total_elements) == 0)
			continue;
		if (x >= 0x180) x -= 0x200;
		if (y >= 0x180) y -= 0x200;

		for (ax = 0; ax < sizex; ax++)
		{
			int sx = flipx ? (x + 16 * (sizex - ax - 1)) : (x + 16 * ax);
			for (ay = 0; ay < sizey; ay++)
			{
				int sy = flipy ? (y + 16 * (sizey - ay - 1)) : (y + 16 * ay);
				drawgfx(r2_sprite_bitmap, &r2_sprite_gfx[pri], code++, color, flipx, flipy, sx, sy,
						cliprect, TRANSPARENCY_PEN, 15);
			}
		}
	}
}

/* 50% mix of two direct 15-bit colours */
#define R2_BLEND(d, s) ((((d) & 0x7bde) >> 1) + (((s) & 0x7bde) >> 1) + ((d) & (s) & 0x0421))

static void r2_blend_layer(struct mame_bitmap *bitmap, const struct rectangle *cliprect, struct mame_bitmap *source, int layer)
{
	const pen_t *pens = Machine->pens;
	int x, y;

	if (layer < 0) return;
	layer <<= 14;
	for (y = cliprect->min_y; y <= cliprect->max_y; y++)
	{
		const UINT16 *src = (const UINT16 *)source->line[y];
		UINT16 *dst = (UINT16 *)bitmap->line[y];
		for (x = cliprect->min_x; x <= cliprect->max_x; x++)
		{
			UINT16 val = src[x];
			if ((val & 0xc000) == layer && (val & 0x000f) != 0x000f)
			{
				val &= 0x07ff;
				if (r2_blend_active[val])
					dst[x] = R2_BLEND(dst[x], pens[val]);
				else
					dst[x] = pens[val];
			}
		}
	}
}

/* the tilemap as palette indexes: render the dirty tiles first, then blit
   through an identity colour table */
static void r2_draw_tilemap_and_blend(struct mame_bitmap *bitmap, const struct rectangle *cliprect, struct tilemap *tmap)
{
	pen_t *saved = Machine->remapped_colortable;
	tilemap_get_pixmap(tmap);
	fillbitmap(r2_tile_bitmap, 0xffff, cliprect);
	Machine->remapped_colortable = r2_identity_pens;
	tilemap_draw(r2_tile_bitmap, cliprect, tmap, 0, 0);
	Machine->remapped_colortable = saved;
	r2_blend_layer(bitmap, cliprect, r2_tile_bitmap, 0);
}

VIDEO_UPDATE(r2play)
{
	int sx, sy;
	unsigned enable = r2_video_enable();

	fillbitmap(bitmap, 0, cliprect);	/* direct RGB black */

	sx = r2_scroll[0] | (r2_scroll[1] << 8); sy = r2_scroll[2] | (r2_scroll[3] << 8);
	tilemap_set_scrollx(r2play_background_layer, 0, sx & 0x1ff); tilemap_set_scrolly(r2play_background_layer, 0, sy & 0x1ff);
	sx = r2_scroll[4] | (r2_scroll[5] << 8); sy = r2_scroll[6] | (r2_scroll[7] << 8);
	tilemap_set_scrollx(r2play_midground_layer, 0, sx & 0x1ff); tilemap_set_scrolly(r2play_midground_layer, 0, sy & 0x1ff);
	sx = r2_scroll[8] | (r2_scroll[9] << 8); sy = r2_scroll[10] | (r2_scroll[11] << 8);
	tilemap_set_scrollx(r2play_foreground_layer, 0, sx & 0x1ff); tilemap_set_scrolly(r2play_foreground_layer, 0, sy & 0x1ff);

	if (!(enable & 0x10))
	{
		r2_draw_sprites_raw(cliprect);
		r2_blend_layer(bitmap, cliprect, r2_sprite_bitmap, r2_cur_spri[0]);
	}
	if (!(enable & 0x01))
		r2_draw_tilemap_and_blend(bitmap, cliprect, r2play_background_layer);
	if (!(enable & 0x10))
		r2_blend_layer(bitmap, cliprect, r2_sprite_bitmap, r2_cur_spri[1]);
	if (!(enable & 0x02))
		r2_draw_tilemap_and_blend(bitmap, cliprect, r2play_midground_layer);
	if (!(enable & 0x10))
		r2_blend_layer(bitmap, cliprect, r2_sprite_bitmap, r2_cur_spri[2]);
	if (!(enable & 0x04))
		r2_draw_tilemap_and_blend(bitmap, cliprect, r2play_foreground_layer);
	if (!(enable & 0x10))
		r2_blend_layer(bitmap, cliprect, r2_sprite_bitmap, r2_cur_spri[3]);
	if (!(enable & 0x08))
		r2_draw_tilemap_and_blend(bitmap, cliprect, r2play_text_layer);
	if (!(enable & 0x10))
		r2_blend_layer(bitmap, cliprect, r2_sprite_bitmap, r2_cur_spri[4]);
}

/* -----------------------------------------------------------------------
 * Main CPU memory
 * ----------------------------------------------------------------------- */

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
	return value;
}

/* Zero Team: P3/P4 at 0x748, cabinet DIP switches in the high byte of 0x74c */
static READ_HANDLER(zeroteam_inputs_r)
{
	switch (offset)
	{
		case 0: return readinputport(2);	/* DSW1 */
		case 1: return readinputport(3);	/* DSW2 */
		case 4: return readinputport(0);	/* P1 */
		case 5: return readinputport(1);	/* P2 */
		case 8: return readinputport(6);	/* P3 */
		case 9: return readinputport(7);	/* P4 */
		case 12: return readinputport(4);	/* SYSTEM */
		case 13: return readinputport(8);	/* SW3 */
	}
	return 0xff;
}

/* Raiden II spreads eight Seibu registers across 32 V30 byte addresses.
   Modern MAME divides its WORD offset by two; our BYTE offset must divide
   by four. Keep the generic Seibu handler unchanged for other games. */
static READ_HANDLER(r2_sound_main_r)
{
 UINT16 word = seibu_main_word_r((offset >> 2) & 7, 0);
 return (word >> ((offset & 1) * 8)) & 0xff;
}

static WRITE_HANDLER(r2_sound_main_w)
{
 /* Only the low byte is wired. Bit 1 of the address is a mirror. */
 if (!(offset & 1)) seibu_main_word_w((offset >> 2) & 7, data & 0xff, 0xff00);
}

/* The tilemaps and the palette are in the private memory of the video chips,
   filled by the COP DMA (modes 0x14 and 0x15): for the CPU, 0x800-0x1ffff
   is plain RAM. */
static MEMORY_READ_START( r2play_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00400, 0x006ff, r2_cop_r },
	{ 0x00700, 0x0071f, r2_sound_main_r },
	{ 0x00740, 0x0074d, r2_inputs_r },
	{ 0x00762, 0x00763, r2_dst1_r },
	{ 0x00000, 0x007ff, MRA_RAM }, /* unassigned low I/O shadow, as on the Vez map */
	{ 0x00800, 0x1ffff, MRA_RAM },
	{ 0x20000, 0x2ffff, MRA_BANK3 },
	{ 0x30000, 0x3ffff, MRA_BANK4 },
	{ 0x40000, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( r2play_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	/* Specific register must precede the overlapping COP range: the old
	 * memory-map builder gives the first matching entry precedence. */
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x00400, 0x006ff, r2_cop_w },
	{ 0x00700, 0x0071f, r2_sound_main_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0bfff, MWA_RAM },
	{ 0x0c000, 0x0cfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0d000, 0x1ffff, MWA_RAM },
	{ 0x20000, 0xfffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( zeroteam_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00400, 0x006ff, r2_cop_r },
	{ 0x00700, 0x0071f, r2_sound_main_r },
	{ 0x00740, 0x0074d, zeroteam_inputs_r },
	{ 0x00762, 0x00763, r2_dst1_r },
	{ 0x00000, 0x007ff, MRA_RAM },
	{ 0x00800, 0x1ffff, MRA_RAM },
	{ 0x20000, 0x2ffff, MRA_BANK3 },
	{ 0x30000, 0x3ffff, MRA_BANK4 },
	{ 0x40000, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( zeroteam_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x00400, 0x006ff, r2_cop_w },
	{ 0x00700, 0x0071f, r2_sound_main_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0efff, MWA_RAM },
	{ 0x0f000, 0x0ffff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x10000, 0x1ffff, MWA_RAM },
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
	else value=readinputport(5);	/* coins */
	return value;
}

static WRITE_HANDLER(r2_sound_z80_reply_w)
{
	seibu_main_data_w(offset,data);
}

static WRITE_HANDLER(r2_sound_z80_pending_w)
{
	seibu_pending_w(offset,data);
}

static MEMORY_READ_START( r2play_sound_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x4009, 0x4009, YM2151_status_port_0_r },
	{ 0x4010, 0x4013, r2_sound_z80_r },
	{ 0x6000, 0x6000, OKIM6295_status_0_r },
	{ 0x6002, 0x6002, OKIM6295_status_1_r },
	{ 0x8000, 0xffff, MRA_BANK1 },
MEMORY_END

static MEMORY_WRITE_START( r2play_sound_writemem )
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

/* Zero Team: YM3812 and one OKI */
static MEMORY_READ_START( zeroteam_sound_readmem )
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x2000, 0x27ff, MRA_RAM },
	{ 0x4008, 0x4008, YM3812_status_port_0_r },
	{ 0x4010, 0x4013, r2_sound_z80_r },
	{ 0x6000, 0x6000, OKIM6295_status_0_r },
	{ 0x8000, 0xffff, MRA_BANK1 },
MEMORY_END

static MEMORY_WRITE_START( zeroteam_sound_writemem )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x27ff, MWA_RAM },
	{ 0x4000, 0x4000, r2_sound_z80_pending_w },
	{ 0x4001, 0x4001, seibu_irq_clear_w },
	{ 0x4002, 0x4002, seibu_rst10_ack_w },
	{ 0x4003, 0x4003, seibu_rst18_ack_w },
	{ 0x4008, 0x4008, YM3812_control_port_0_w },
	{ 0x4009, 0x4009, YM3812_write_port_0_w },
	{ 0x4018, 0x4019, r2_sound_z80_reply_w },
	{ 0x401a, 0x401a, seibu_bank_w },
	{ 0x401b, 0x401b, seibu_coin_w },
	{ 0x6000, 0x6000, OKIM6295_data_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

static struct YM2151interface r2play_ym2151_interface =
{
	1,
	28636360/8,
	{ YM3012_VOL(70,MIXER_PAN_CENTER,70,MIXER_PAN_CENTER) },
	{ seibu_ym2151_irqhandler }
};

static struct OKIM6295interface r2play_oki_interface =
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
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT(0xc0, IP_ACTIVE_LOW, IPT_UNUSED)

	PORT_START /* P2 high byte of modern P1_P2 */
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
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
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 4 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 4 )
	PORT_BIT(0xfc, IP_ACTIVE_LOW, IPT_UNUSED)
INPUT_PORTS_END

#define construct_ipt_r2play construct_ipt_raiden2

INPUT_PORTS_START( raidendx )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* Dip switch A  */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ))
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ))
	PORT_DIPSETTING(    0x08, DEF_STR( 4C_1C ))
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ))
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ))
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ))
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ))
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ))
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_4C ))
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x40, 0x40, "Starting Coin" )
	PORT_DIPSETTING(    0x40, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, "X 2" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch B  */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ))
	PORT_DIPSETTING(    0x03, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ))
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) ) /* Manual shows "Not Used" */
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) ) /* Manual shows "Not Used" */
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Demo Sound" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ))
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Test Mode" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* START BUTTONS */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* coins read by sound CPU */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 4 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 4 )
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* Zero Team: four players; coins, difficulty, lives and bonus differ from Raiden II */
INPUT_PORTS_START( zeroteam )
	PORT_START	/* 0: P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1: P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 2: DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, "Starting Coin" )
	PORT_DIPSETTING(    0x40, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, "X 2" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* 3: DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "1000000" )
	PORT_DIPSETTING(    0x20, "2000000" )
	PORT_DIPSETTING(    0x10, "Every 1000000" )
	PORT_DIPSETTING(    0x00, "No Extend" )
	PORT_DIPNAME( 0x40, 0x40, "Demo Sound" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* 4: SYSTEM */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 5: coins, read by the sound CPU */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 4 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 4 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 4 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN4, 4 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 6: P3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 7: P4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 8: SW3, cabinet */
	PORT_DIPNAME( 0x07, 0x07, "Cabinet Setting" )
	PORT_DIPSETTING(    0x07, "2P" )
	PORT_DIPSETTING(    0x06, "3P 3Slot" )
	PORT_DIPSETTING(    0x05, "4P 4Slot" )
	PORT_DIPSETTING(    0x04, "3P 2Slot" )
	PORT_DIPSETTING(    0x03, "2P x2" )
	PORT_DIPSETTING(    0x02, "4P 2Slot" )
	PORT_DIPSETTING(    0x01, "2P Freeplay" )
	PORT_DIPSETTING(    0x00, "4P Freeplay" )
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( nzeroteam )
	PORT_START /* port 0: coins read through sound cpu */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 4 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 4 )
	PORT_BIT( 0xfc, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* port 1: SYSTEM */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0xf8, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* port 2: P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* port 3: P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* port 4: DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, "Starting Coin" )
	PORT_DIPSETTING(    0x40, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x00, "X 2" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* port 5: DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Normal ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x0c, "2" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "2000000" )
	PORT_DIPSETTING(    0x20, "1000000" )
	PORT_DIPSETTING(    0x10, "3000000" )
	PORT_DIPSETTING(    0x00, "No Extend" )
	PORT_DIPNAME( 0x40, 0x40, "Demo Sound" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END

INPUT_PORTS_START( r2dx_v33 )
	PORT_START /* 0: unused */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 1: SYSTEM */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* EEPROM bit */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x00, DEF_STR( On ) )

	PORT_START /* 2: P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START /* 3: P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )
INPUT_PORTS_END

INPUT_PORTS_START( zerotm2k )
	PORT_START /* 0: COIN */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_LOW, IPT_COIN1, 4 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_LOW, IPT_COIN2, 4 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_LOW, IPT_COIN3, 4 )
	PORT_BIT_IMPULSE( 0x08, IP_ACTIVE_LOW, IPT_COIN4, 4 )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 1: SYSTEM */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE( 0x20, IP_ACTIVE_LOW )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 2: P1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 3: P2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* EEPROM bit */

	PORT_START /* 4: P3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER3 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* 5: P4 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

/* -----------------------------------------------------------------------
 * Graphics decode
 * ----------------------------------------------------------------------- */

static struct GfxLayout r2play_charlayout =
{
	8,8,4096,4,
	{8,12,0,4},
	{3,2,1,0,19,18,17,16},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32},
	32*8
};

static struct GfxLayout r2dx_charlayout =
{
	8,8,8192,4,
	{8,12,0,4},
	{3,2,1,0,19,18,17,16},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32},
	32*8
};

static struct GfxLayout r2play_tilelayout =
{
	16,16,0x8000,4,
	{8,12,0,4},
	{3,2,1,0,19,18,17,16,3+64*8,2+64*8,1+64*8,0+64*8,19+64*8,18+64*8,17+64*8,16+64*8},
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,8*32,9*32,10*32,11*32,12*32,13*32,14*32,15*32},
	128*8
};

static struct GfxLayout r2play_spritelayout =
{
	16,16,0x10000,4,
	{0,1,2,3},
	{4,0,12,8,20,16,28,24,36,32,44,40,52,48,60,56},
	{0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64,8*64,9*64,10*64,11*64,12*64,13*64,14*64,15*64},
	16*16*4
};

static struct GfxDecodeInfo r2play_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &r2play_charlayout,   0x700, 0x10 },
	{ REGION_GFX2, 0x00000, &r2play_tilelayout,   0x400, 0x40 },
	{ REGION_GFX3, 0x00000, &r2play_spritelayout, 0x000, 0x40 },
	{ -1 }
};

static struct GfxDecodeInfo r2dx_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &r2dx_charlayout,     0x700, 0x10 },
	{ REGION_GFX2, 0x00000, &r2play_tilelayout,   0x400, 0x40 },
	{ REGION_GFX3, 0x00000, &r2play_spritelayout, 0x000, 0x40 },
	{ -1 }
};

/* -----------------------------------------------------------------------
 * Machine
 * ----------------------------------------------------------------------- */

static INTERRUPT_GEN( r2play_interrupt )
{
	cpu_set_irq_line_and_vector(cpu_getactivecpu(), 0, HOLD_LINE, 0xc0/4);
}

/* MAME: 32 MHz / 4 pixel clock, 512 x 282 total */
#define R2_REFRESH		(32000000.0 / 4 / 512 / 282)
#define R2_LINE_US		(1000000.0 / (R2_REFRESH * 282))

static void r2_sound_reset(void)
{
	machine_init_seibu_sound_1();
	seibu_bank_w(0, 0);
}

static void r2_common_reset(int game, const int *spri, int fg_bank, int mid_bank)
{
	r2_game = game;
	r2_is_r2dx_v33 = 0;
	r2_is_raidendx = (game == R2_GAME_RAIDENDX);
	r2_cur_spri = spri;

	r2play_bg_bank = 0;
	r2play_mid_bank = mid_bank;
	r2play_fg_bank = fg_bank;
	r2play_tx_bank = 0;
	if (r2play_background_layer) tilemap_mark_all_tiles_dirty(r2play_background_layer);
	if (r2play_midground_layer) tilemap_mark_all_tiles_dirty(r2play_midground_layer);
	if (r2play_foreground_layer) tilemap_mark_all_tiles_dirty(r2play_foreground_layer);
	if (r2play_text_layer) tilemap_mark_all_tiles_dirty(r2play_text_layer);
	memset(r2_scroll, 0, sizeof(r2_scroll));
	r2_layer_enable = 0;
	r2_prg_bank = 0;
	r2_reset_cop();
	if (buffered_spriteram)
		memset(buffered_spriteram, 0, spriteram_size);
}

static MACHINE_INIT(raiden2)
{
	r2_common_reset(R2_GAME_RAIDEN2, r2_default_spri, 6, 1);
	r2_sound_reset();
	/* bank entry 1, as in MAME; BANK3/BANK4, since the sound CPU uses BANK1 */
	if (r2_bank_rom)
		r2_main_bankswitch(0);
}

static MACHINE_INIT(raidendx)
{
	r2_common_reset(R2_GAME_RAIDENDX, r2_default_spri, 6, 1);
	r2_sound_reset();
	r2_dx_bankswitch(0);
}

static MACHINE_INIT(zeroteam)
{
	r2_common_reset(R2_GAME_ZEROTEAM, r2_zeroteam_spri, 2, 1);
	r2_sound_reset();
	if (r2_bank_rom)
		r2_main_bankswitch(0);
}

static MACHINE_DRIVER_START( raiden2 )
	MDRV_CPU_ADD(V30, 32000000/2)
	MDRV_CPU_MEMORY(r2play_readmem, r2play_writemem)
	MDRV_CPU_VBLANK_INT(r2play_interrupt, 1)

	MDRV_CPU_ADD(Z80, 28636360/8)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(r2play_sound_readmem, r2play_sound_writemem)

	MDRV_FRAMES_PER_SECOND(R2_REFRESH)
	MDRV_VBLANK_DURATION((282 - 240) * R2_LINE_US)
	MDRV_MACHINE_INIT(raiden2)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(r2play_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(r2play)
	MDRV_VIDEO_UPDATE(r2play)

	/* sound hardware */
	MDRV_SOUND_ADD(YM2151, r2play_ym2151_interface)
	MDRV_SOUND_ADD(OKIM6295, r2play_oki_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( raidendx )
	MDRV_IMPORT_FROM(raiden2)
	MDRV_MACHINE_INIT(raidendx)
MACHINE_DRIVER_END

static struct YM3812interface zeroteam_ym3812_interface =
{
	1,
	28636360/8,
	{ 100 },
	{ seibu_ym3812_irqhandler }
};

static struct OKIM6295interface zeroteam_oki_interface =
{
	1,
	{ (28636360/28)/132 },
	{ REGION_SOUND1 },
	{ 40 }
};

static MACHINE_DRIVER_START( zeroteam )
	MDRV_CPU_ADD(V30, 32000000/2)
	MDRV_CPU_MEMORY(zeroteam_readmem, zeroteam_writemem)
	MDRV_CPU_VBLANK_INT(r2play_interrupt, 1)

	MDRV_CPU_ADD(Z80, 28636360/8)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(zeroteam_sound_readmem, zeroteam_sound_writemem)

	MDRV_FRAMES_PER_SECOND(R2_REFRESH)
	MDRV_VBLANK_DURATION((282 - 256) * R2_LINE_US)
	MDRV_MACHINE_INIT(zeroteam)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_RGB_DIRECT | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 32*8-1)
	MDRV_GFXDECODE(r2play_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(r2play)
	MDRV_VIDEO_UPDATE(r2play)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, zeroteam_ym3812_interface)
	MDRV_SOUND_ADD(OKIM6295, zeroteam_oki_interface)
MACHINE_DRIVER_END

/* -----------------------------------------------------------------------
 * New Zero Team (V33 SYSTEM TYPE_B hardware with SEI333 / COPX-D3)
 * ----------------------------------------------------------------------- */

static UINT16 nzt_i_dx, nzt_i_dy, nzt_i_angle;
static UINT32 nzt_i_sdist;

static WRITE_HANDLER(nzt_math_w)
{
	switch (offset)
	{
		case 0x00: nzt_i_dx = (nzt_i_dx & 0xff00) | data; break;
		case 0x01: nzt_i_dx = (nzt_i_dx & 0x00ff) | ((UINT16)data << 8); break;
		case 0x02: nzt_i_dy = (nzt_i_dy & 0xff00) | data; break;
		case 0x03: nzt_i_dy = (nzt_i_dy & 0x00ff) | ((UINT16)data << 8); break;
		case 0x04: nzt_i_sdist = (nzt_i_sdist & 0xffffff00U) | data; break;
		case 0x05: nzt_i_sdist = (nzt_i_sdist & 0xffff00ffU) | ((UINT32)data << 8); break;
		case 0x06: nzt_i_sdist = (nzt_i_sdist & 0xff00ffffU) | ((UINT32)data << 16); break;
		case 0x07: nzt_i_sdist = (nzt_i_sdist & 0x00ffffffU) | ((UINT32)data << 24); break;
		case 0x08: nzt_i_angle = (nzt_i_angle & 0xff00) | data; break;
		case 0x09: nzt_i_angle = (nzt_i_angle & 0x00ff) | ((UINT16)data << 8); break;
	}
}

static READ_HANDLER(nzt_math_r)
{
	UINT8 *math = memory_region(REGION_USER2);
	switch (offset)
	{
		case 0x00:
		{
			UINT16 val = math ? math[((nzt_i_dy & 0xff) << 8) | (nzt_i_dx & 0xff)] : 0;
			return val & 0xff;
		}
		case 0x01:
		{
			UINT16 val = math ? math[((nzt_i_dy & 0xff) << 8) | (nzt_i_dx & 0xff)] : 0;
			return (val >> 8) & 0xff;
		}
		case 0x02:
		{
			UINT16 val = (UINT16)sqrt((double)nzt_i_sdist);
			return val & 0xff;
		}
		case 0x03:
		{
			UINT16 val = (UINT16)sqrt((double)nzt_i_sdist);
			return (val >> 8) & 0xff;
		}
		case 0x04:
		{
			int off = 65536 + (nzt_i_angle & 0xff) * 4;
			UINT16 val = math ? ((UINT16)math[off] | ((UINT16)math[off + 1] << 8)) : 0;
			return val & 0xff;
		}
		case 0x05:
		{
			int off = 65536 + (nzt_i_angle & 0xff) * 4;
			UINT16 val = math ? ((UINT16)math[off] | ((UINT16)math[off + 1] << 8)) : 0;
			return (val >> 8) & 0xff;
		}
		case 0x06:
		{
			int off = 65536 + (nzt_i_angle & 0xff) * 4;
			UINT16 val = math ? ((UINT16)math[off + 2] | ((UINT16)math[off + 3] << 8)) : 0;
			return val & 0xff;
		}
		case 0x07:
		{
			int off = 65536 + (nzt_i_angle & 0xff) * 4;
			UINT16 val = math ? ((UINT16)math[off + 2] | ((UINT16)math[off + 3] << 8)) : 0;
			return (val >> 8) & 0xff;
		}
		default:
			return 0;
	}
}

/* tilemaps from 0xd000 and palette from 0x1f000 to the private buffers */
static WRITE_HANDLER(nzt_tilemapdma_w)
{
	UINT32 i;
	if (offset != 1) return;
	for (i = 0; i < 0x2800 / 2; i++)
		r2_tile_w(i, r2_mem_r16(0xd000 + i * 2));
}

static WRITE_HANDLER(nzt_paldma_w)
{
	UINT32 i;
	if (offset != 1) return;
	for (i = 0; i < 0x1000 / 2; i++)
		r2_palette_w(i, r2_mem_r16(0x1f000 + i * 2));
}

static READ_HANDLER(r2_crtc_r)
{
	return r2_cop_r(0x200 + offset);
}

static WRITE_HANDLER(r2_crtc_w)
{
	r2_cop_w(0x200 + offset, data);
}

static READ_HANDLER(r2_cop_6c0_r)
{
	return r2_cop_r(0x2c0 + offset);
}

static WRITE_HANDLER(r2_cop_6c0_w)
{
	r2_cop_w(0x2c0 + offset, data);
}

static READ_HANDLER(nzt_inputs_r)
{
	switch (offset)
	{
		case 0: return readinputport(4); /* DSW1 */
		case 1: return readinputport(5); /* DSW2 */
		case 4: return readinputport(2); /* P1 */
		case 5: return readinputport(3); /* P2 */
		case 12: return readinputport(1); /* SYSTEM */
		default: return 0xff;
	}
}

static MEMORY_READ_START( nzeroteam_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00430, 0x00437, nzt_math_r },
	{ 0x00600, 0x0064f, r2_crtc_r },
	{ 0x00740, 0x0074f, nzt_inputs_r },
	{ 0x00780, 0x0079f, r2_sound_main_r },
	{ 0x00000, 0x007ff, MRA_RAM },
	{ 0x00800, 0x0bfff, MRA_RAM },
	{ 0x0c000, 0x0cfff, MRA_RAM },
	{ 0x0d000, 0x0f7ff, MRA_RAM },
	{ 0x0f800, 0x0ffff, MRA_RAM },
	{ 0x10000, 0x1efff, MRA_RAM },
	{ 0x1f000, 0x1ffff, MRA_RAM },
	{ 0x20000, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( nzeroteam_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	{ 0x00400, 0x00401, nzt_tilemapdma_w },
	{ 0x00402, 0x00403, nzt_paldma_w },
	{ 0x00406, 0x00407, MWA_NOP },
	{ 0x00420, 0x00429, nzt_math_w },
	{ 0x00600, 0x0064f, r2_crtc_w },
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x006b0, 0x006bd, MWA_NOP },
	{ 0x00780, 0x0079f, r2_sound_main_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0bfff, MWA_RAM },
	{ 0x0c000, 0x0cfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0d000, 0x0f7ff, MWA_RAM },
	{ 0x0f800, 0x0ffff, MWA_RAM },
	{ 0x10000, 0x1efff, MWA_RAM },
	{ 0x1f000, 0x1ffff, MWA_RAM },
	{ 0x20000, 0xfffff, MWA_ROM },
MEMORY_END

static MEMORY_WRITE_START( nzeroteam_sound_writemem )
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2000, 0x27ff, MWA_RAM },
	{ 0x4000, 0x4000, seibu_pending_w },
	{ 0x4001, 0x4001, seibu_irq_clear_w },
	{ 0x4002, 0x4002, seibu_rst10_ack_w },
	{ 0x4003, 0x4003, seibu_rst18_ack_w },
	{ 0x4007, 0x4007, seibu_bank_w },
	{ 0x4008, 0x4008, YM3812_control_port_0_w },
	{ 0x4009, 0x4009, YM3812_write_port_0_w },
	{ 0x4018, 0x4019, seibu_main_data_w },
	{ 0x401a, 0x401a, seibu_bank_w },
	{ 0x401b, 0x401b, seibu_coin_w },
	{ 0x6000, 0x6000, OKIM6295_data_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
MEMORY_END

static MACHINE_INIT(nzeroteam)
{
	r2_common_reset(R2_GAME_V33, r2_zeroteam_spri, 2, 1);
	r2_sound_reset();
	nzt_i_dx = nzt_i_dy = nzt_i_angle = 0;
	nzt_i_sdist = 0;
}

static struct YM3812interface nzt_ym3812_interface =
{
	1,
	14318180/4,
	{ 100 },
	{ seibu_ym3812_irqhandler }
};

static struct OKIM6295interface nzt_oki_interface =
{
	1,
	{ (28636360/28)/132 },
	{ REGION_SOUND1 },
	{ 80 }
};

static struct OKIM6295interface r2dx_v33_oki_interface =
{
	1,
	{ (28636360/28)/132 },
	{ REGION_SOUND1 },
	{ 100 }
};

static MACHINE_DRIVER_START( nzeroteam )
	MDRV_CPU_ADD(V33, 32000000/2)
	MDRV_CPU_MEMORY(nzeroteam_readmem, nzeroteam_writemem)
	MDRV_CPU_VBLANK_INT(r2play_interrupt, 1)

	MDRV_CPU_ADD(Z80, 14318180/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(seibu_sound_readmem, nzeroteam_sound_writemem)

	MDRV_FRAMES_PER_SECOND(55.47)
	MDRV_VBLANK_DURATION(500)
	MDRV_MACHINE_INIT(nzeroteam)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_RGB_DIRECT | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(44*8, 34*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 32*8-1)
	MDRV_GFXDECODE(r2play_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(r2play)
	MDRV_VIDEO_UPDATE(r2play)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, nzt_ym3812_interface)
	MDRV_SOUND_ADD(OKIM6295, nzt_oki_interface)
MACHINE_DRIVER_END

static int m_r2dxbank = 0;
static int m_r2dxgameselect = 0;
static int r2dx_okibank;

static void r2dx_setbanking(void)
{
	UINT8 *rom = memory_region(REGION_USER1);
	if (!rom) return;
	cpu_setbank(5, rom + (m_r2dxgameselect * 0x20 + m_r2dxbank + 16) * 0x10000);
	cpu_setbank(6, rom + (m_r2dxgameselect ? 0x230000 : 0x030000));
}

static WRITE_HANDLER(r2dx_rom_bank_w)
{
	if (offset == 0)
	{
		m_r2dxbank = data & 0x0f;
		r2dx_setbanking();
	}
}

static WRITE_HANDLER(r2dx_tile_bank_w)
{
	if (offset == 0)
	{
		int new_bank;
		new_bank = ((data & 0x10) >> 4);
		if (new_bank != r2play_bg_bank) {
			r2play_bg_bank = new_bank;
			if (r2play_background_layer) tilemap_mark_all_tiles_dirty(r2play_background_layer);
		}

		new_bank = 2 + ((data & 0x20) >> 5);
		if (new_bank != r2play_mid_bank) {
			r2play_mid_bank = new_bank;
			if (r2play_midground_layer) tilemap_mark_all_tiles_dirty(r2play_midground_layer);
		}

		new_bank = 4 | (data & 3);
		if (new_bank != r2play_fg_bank) {
			r2play_fg_bank = new_bank;
			if (r2play_foreground_layer) tilemap_mark_all_tiles_dirty(r2play_foreground_layer);
		}
	}
}

static WRITE_HANDLER(r2dx_v33_eeprom_w)
{
	if (offset == 0)
	{
		EEPROM_set_clock_line((data & 0x10) ? ASSERT_LINE : CLEAR_LINE);
		EEPROM_write_bit((data & 0x20) >> 5);
		EEPROM_set_cs_line((data & 0x08) ? CLEAR_LINE : ASSERT_LINE);

		m_r2dxgameselect = (data & 0x04) >> 2;
		r2play_tx_bank = m_r2dxgameselect;
		if (r2play_text_layer) tilemap_mark_all_tiles_dirty(r2play_text_layer);
		r2dx_setbanking();
		r2dx_okibank = data & 3;
		OKIM6295_set_bank_base(0, r2dx_okibank * 0x40000);
	}
}

static READ_HANDLER(r2dx_v33_inputs_r)
{
	switch (offset)
	{
		case 0:
		case 1:
			return 0xff; /* 0x740: r2dx_debug_r (needed for collision) */
		case 4:
			return readinputport(2); /* P1 */
		case 5:
			return readinputport(3); /* P2 */
		case 12: /* 0x74c: SYSTEM + EEPROM DO at bit 4 (0x10) */
		{
			UINT8 sys = readinputport(1) & ~0x10;
			int bit = EEPROM_read_bit();
			if (bit) sys |= 0x10;
			return sys;
		}
		default:
			return 0xff;
	}
}

static WRITE_HANDLER(zerotm2k_eeprom_w)
{
	if (offset == 0)
	{
		EEPROM_set_clock_line((data & 0x02) ? ASSERT_LINE : CLEAR_LINE);
		EEPROM_write_bit((data & 0x04) >> 2);
		EEPROM_set_cs_line((data & 0x01) ? CLEAR_LINE : ASSERT_LINE);
	}
}

static READ_HANDLER(zerotm2k_inputs_r)
{
	switch (offset)
	{
		case 0: return readinputport(4); /* 0x740: P3 */
		case 1: return readinputport(5); /* 0x741: P4 */
		case 4: return readinputport(2); /* 0x744: P1 */
		case 5: /* 0x745: P2 + EEPROM DO at bit 7 (0x80) */
		{
			UINT8 p2 = readinputport(3) & ~0x80;
			if (EEPROM_read_bit()) p2 |= 0x80;
			return p2;
		}
		case 12: return readinputport(1); /* 0x74c: SYSTEM */
		default: return 0xff;
	}
}

static MEMORY_READ_START( r2dx_v33_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00430, 0x00437, nzt_math_r },
	{ 0x00600, 0x0064f, r2_crtc_r },
	{ 0x006c0, 0x006ff, r2_cop_6c0_r },
	{ 0x00740, 0x0074f, r2dx_v33_inputs_r },
	{ 0x00762, 0x00763, r2_dst1_r },
	{ 0x00780, 0x00780, OKIM6295_status_0_r },
	{ 0x00000, 0x007ff, MRA_RAM },
	{ 0x00800, 0x0bfff, MRA_RAM },
	{ 0x0c000, 0x0cfff, MRA_RAM },
	{ 0x0d000, 0x0f7ff, MRA_RAM },
	{ 0x0f800, 0x0ffff, MRA_RAM },
	{ 0x10000, 0x1efff, MRA_RAM },
	{ 0x1f000, 0x1ffff, MRA_RAM },
	{ 0x20000, 0x2ffff, MRA_BANK5 },
	{ 0x30000, 0xfffff, MRA_BANK6 },
MEMORY_END

static MEMORY_WRITE_START( r2dx_v33_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	{ 0x00400, 0x00401, nzt_tilemapdma_w },
	{ 0x00402, 0x00403, nzt_paldma_w },
	{ 0x00404, 0x00405, r2dx_rom_bank_w },
	{ 0x00406, 0x00407, r2dx_tile_bank_w },
	{ 0x00420, 0x00429, nzt_math_w },
	{ 0x00600, 0x0064f, r2_crtc_w },
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x006b0, 0x006bd, MWA_NOP },
	{ 0x006c0, 0x006ff, r2_cop_6c0_w },
	{ 0x00700, 0x00701, r2dx_v33_eeprom_w },
	{ 0x00780, 0x00780, OKIM6295_data_0_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0bfff, MWA_RAM },
	{ 0x0c000, 0x0cfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0d000, 0x0f7ff, MWA_RAM },
	{ 0x0f800, 0x0ffff, MWA_RAM },
	{ 0x10000, 0x1efff, MWA_RAM },
	{ 0x1f000, 0x1ffff, MWA_RAM },
	{ 0x20000, 0xfffff, MWA_ROM },
MEMORY_END

static MEMORY_READ_START( zerotm2k_readmem )
	{ 0x00000, 0x003ff, MRA_RAM },
	{ 0x00430, 0x00437, nzt_math_r },
	{ 0x00600, 0x0064f, r2_crtc_r },
	{ 0x00740, 0x0074f, zerotm2k_inputs_r },
	{ 0x00780, 0x0079f, r2_sound_main_r },
	{ 0x00000, 0x007ff, MRA_RAM },
	{ 0x00800, 0x0bfff, MRA_RAM },
	{ 0x0c000, 0x0cfff, MRA_RAM },
	{ 0x0d000, 0x0f7ff, MRA_RAM },
	{ 0x0f800, 0x0ffff, MRA_RAM },
	{ 0x10000, 0x1efff, MRA_RAM },
	{ 0x1f000, 0x1ffff, MRA_RAM },
	{ 0x20000, 0xfffff, MRA_ROM },
MEMORY_END

static MEMORY_WRITE_START( zerotm2k_writemem )
	{ 0x00000, 0x003ff, MWA_RAM },
	{ 0x00400, 0x00401, nzt_tilemapdma_w },
	{ 0x00402, 0x00403, nzt_paldma_w },
	{ 0x00406, 0x00407, MWA_NOP },
	{ 0x00420, 0x00429, nzt_math_w },
	{ 0x00600, 0x0064f, r2_crtc_w },
	{ 0x0068e, 0x0068f, buffer_spriteram_w },
	{ 0x006b0, 0x006bd, MWA_NOP },
	{ 0x00748, 0x00749, zerotm2k_eeprom_w },
	{ 0x00780, 0x0079f, r2_sound_main_w },
	{ 0x00000, 0x007ff, MWA_RAM },
	{ 0x00800, 0x0bfff, MWA_RAM },
	{ 0x0c000, 0x0cfff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0d000, 0x0f7ff, MWA_RAM },
	{ 0x0f800, 0x0ffff, MWA_RAM },
	{ 0x10000, 0x1efff, MWA_RAM },
	{ 0x1f000, 0x1ffff, MWA_RAM },
	{ 0x20000, 0xfffff, MWA_ROM },
MEMORY_END

static NVRAM_HANDLER( r2dx_v33 )
{
	if (read_or_write)
		EEPROM_save(file);
	else
	{
		EEPROM_init(&eeprom_interface_93C46);
		if (file)
			EEPROM_load(file);
		else if (memory_region(REGION_USER3))
			EEPROM_set_data(memory_region(REGION_USER3), 0x80);
	}
}

static MACHINE_INIT(r2dx_v33)
{
	r2_common_reset(R2_GAME_V33, r2_default_spri, 6, 2);
	m_r2dxbank = 0;
	m_r2dxgameselect = 0;
	nzt_i_dx = nzt_i_dy = nzt_i_angle = 0;
	nzt_i_sdist = 0;
	r2_is_r2dx_v33 = 1;
	r2dx_okibank = 0;
	r2dx_setbanking();
	OKIM6295_set_bank_base(0, 0);
}

static MACHINE_DRIVER_START( r2dx_v33 )
	MDRV_CPU_ADD(V33, 32000000/2)
	MDRV_CPU_MEMORY(r2dx_v33_readmem, r2dx_v33_writemem)
	MDRV_CPU_VBLANK_INT(r2play_interrupt, 1)

	MDRV_FRAMES_PER_SECOND(55.47)
	MDRV_VBLANK_DURATION(500)
	MDRV_MACHINE_INIT(r2dx_v33)
	MDRV_NVRAM_HANDLER(r2dx_v33)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_RGB_DIRECT | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(44*8, 34*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 30*8-1)
	MDRV_GFXDECODE(r2dx_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(r2play)
	MDRV_VIDEO_UPDATE(r2play)

	/* sound hardware */
	MDRV_SOUND_ADD(OKIM6295, r2dx_v33_oki_interface)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( zerotm2k )
	MDRV_CPU_ADD(V33, 32000000/2)
	MDRV_CPU_MEMORY(zerotm2k_readmem, zerotm2k_writemem)
	MDRV_CPU_VBLANK_INT(r2play_interrupt, 1)

	MDRV_CPU_ADD(Z80, 14318180/4)
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_MEMORY(seibu_sound_readmem, nzeroteam_sound_writemem)

	MDRV_FRAMES_PER_SECOND(55.47)
	MDRV_VBLANK_DURATION(500)
	MDRV_MACHINE_INIT(nzeroteam)
	MDRV_NVRAM_HANDLER(93C46)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_BUFFERS_SPRITERAM | VIDEO_RGB_DIRECT | VIDEO_UPDATE_AFTER_VBLANK)
	MDRV_SCREEN_SIZE(44*8, 34*8)
	MDRV_VISIBLE_AREA(0*8, 40*8-1, 0*8, 32*8-1)
	MDRV_GFXDECODE(r2play_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)
	MDRV_VIDEO_START(r2play)
	MDRV_VIDEO_UPDATE(r2play)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, nzt_ym3812_interface)
	MDRV_SOUND_ADD(OKIM6295, nzt_oki_interface)
MACHINE_DRIVER_END

#define construct_r2play construct_raiden2

/* -----------------------------------------------------------------------
 * Save states
 * ----------------------------------------------------------------------- */

static void r2_state_postload(void)
{
	int i;

	if (r2_game == R2_GAME_RAIDENDX)
		r2_dx_bankswitch(r2_dx_bank);
	else if (r2_game == R2_GAME_V33)
	{
		if (r2_is_r2dx_v33)
		{
			r2dx_setbanking();
			OKIM6295_set_bank_base(0, r2dx_okibank * 0x40000);
		}
	}
	else if (r2_bank_rom)
		r2_main_bankswitch(r2_prg_bank);

	for (i = 0; i < 0x1000 / 2; i++)
	{
		UINT16 data = r2_palette_data[i];
		int r = data & 0x1f, g = (data >> 5) & 0x1f, b = (data >> 10) & 0x1f;
		palette_set_color(i, (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
	}
	tilemap_mark_all_tiles_dirty(r2play_background_layer);
	tilemap_mark_all_tiles_dirty(r2play_midground_layer);
	tilemap_mark_all_tiles_dirty(r2play_foreground_layer);
	tilemap_mark_all_tiles_dirty(r2play_text_layer);
}

static void r2_state_register(void)
{
	int i;
	const char *m = "raiden2";

	state_save_register_UINT16(m, 0, "back", r2_back_data, 0x800/2);
	state_save_register_UINT16(m, 0, "fore", r2_fore_data, 0x800/2);
	state_save_register_UINT16(m, 0, "mid", r2_mid_data, 0x800/2);
	state_save_register_UINT16(m, 0, "text", r2_text_data, 0x1000/2);
	state_save_register_UINT16(m, 0, "palette", r2_palette_data, 0x1000/2);
	state_save_register_int(m, 0, "bg_bank", &r2play_bg_bank);
	state_save_register_int(m, 0, "mid_bank", &r2play_mid_bank);
	state_save_register_int(m, 0, "fg_bank", &r2play_fg_bank);
	state_save_register_int(m, 0, "tx_bank", &r2play_tx_bank);
	state_save_register_UINT8(m, 0, "scroll", r2_scroll, 12);
	state_save_register_UINT16(m, 0, "layer_enable", &r2_layer_enable, 1);
	state_save_register_UINT16(m, 0, "prg_bank", &r2_prg_bank, 1);
	state_save_register_int(m, 0, "dx_bank", &r2_dx_bank);

	m = "raiden2_cop";
	state_save_register_UINT8(m, 0, "ram", r2_copram, sizeof(r2_copram));
	state_save_register_UINT32(m, 0, "regs", r2_cop_regs, 8);
	state_save_register_UINT32(m, 0, "itoa", &r2_cop_itoa, 1);
	state_save_register_UINT16(m, 0, "status", &r2_cop_status, 1);
	state_save_register_UINT16(m, 0, "scale", &r2_cop_scale, 1);
	state_save_register_UINT16(m, 0, "itoa_digit_count", &r2_cop_itoa_digit_count, 1);
	state_save_register_UINT8(m, 0, "itoa_digits", r2_cop_itoa_digits, 10);
	state_save_register_UINT16(m, 0, "angle", &r2_cop_angle, 1);
	state_save_register_UINT16(m, 0, "dist", &r2_cop_dist, 1);
	state_save_register_UINT16(m, 0, "angle_target", &r2_cop_angle_target, 1);
	state_save_register_UINT16(m, 0, "angle_step", &r2_cop_angle_step, 1);
	state_save_register_UINT16(m, 0, "dma_mode", &r2_cop_dma_mode, 1);
	state_save_register_UINT16(m, 0, "dma_src", r2_cop_dma_src, 0x200);
	state_save_register_UINT16(m, 0, "dma_dst", r2_cop_dma_dst, 0x200);
	state_save_register_UINT16(m, 0, "dma_size", r2_cop_dma_size, 0x200);
	state_save_register_UINT16(m, 0, "dma_v1", &r2_cop_dma_v1, 1);
	state_save_register_UINT16(m, 0, "dma_v2", &r2_cop_dma_v2, 1);
	state_save_register_UINT16(m, 0, "dma_adr_rel", &r2_cop_dma_adr_rel, 1);
	state_save_register_UINT16(m, 0, "brightness", &r2_pal_brightness_val, 1);
	state_save_register_UINT16(m, 0, "brightness_mode", &r2_pal_brightness_mode, 1);
	state_save_register_UINT16(m, 0, "rng_max", &r2_cop_rng_max, 1);
	state_save_register_UINT16(m, 0, "bank", &r2_cop_bank, 1);
	state_save_register_UINT16(m, 0, "sprite_x", &r2_sprite_prot_x, 1);
	state_save_register_UINT16(m, 0, "sprite_y", &r2_sprite_prot_y, 1);
	state_save_register_UINT16(m, 0, "dst1", &r2_dst1, 1);
	state_save_register_UINT16(m, 0, "sprite_maxx", &r2_cop_spr_maxx, 1);
	state_save_register_UINT16(m, 0, "sprite_off", &r2_cop_spr_off, 1);
	state_save_register_UINT16(m, 0, "sprite_src", r2_sprite_prot_src_addr, 2);
	state_save_register_UINT16(m, 0, "hit_status", &r2_cop_hit_status, 1);
	state_save_register_UINT16(m, 0, "hit_base", &r2_cop_hit_baseadr, 1);
	state_save_register_UINT16(m, 0, "hit_value_status", &r2_cop_hit_val_stat, 1);
	state_save_register_INT16(m, 0, "hit_values", r2_cop_hit_val, 3);
	state_save_register_UINT32(m, 0, "sort_ram", &r2_cop_sort_ram_addr, 1);
	state_save_register_UINT32(m, 0, "sort_lookup", &r2_cop_sort_lookup, 1);
	state_save_register_UINT16(m, 0, "sort_param", &r2_cop_sort_param, 1);
	for (i = 0; i < 2; i++)
	{
		state_save_register_INT16("raiden2_collision", i, "pos", r2_collision[i].pos, 3);
		state_save_register_INT8("raiden2_collision", i, "delta", r2_collision[i].dx, 3);
		state_save_register_UINT8("raiden2_collision", i, "size", r2_collision[i].size, 3);
		state_save_register_int("raiden2_collision", i, "allow_swap", &r2_collision[i].allow_swap);
		state_save_register_UINT16("raiden2_collision", i, "flags_swap", &r2_collision[i].flags_swap, 1);
		state_save_register_UINT32("raiden2_collision", i, "sprite_address", &r2_collision[i].spradr, 1);
		state_save_register_INT16("raiden2_collision", i, "minimum", r2_collision[i].min, 3);
		state_save_register_INT16("raiden2_collision", i, "maximum", r2_collision[i].max, 3);
	}

	m = "raiden2_v33";
	state_save_register_UINT16(m, 0, "dx", &nzt_i_dx, 1);
	state_save_register_UINT16(m, 0, "dy", &nzt_i_dy, 1);
	state_save_register_UINT16(m, 0, "angle", &nzt_i_angle, 1);
	state_save_register_UINT32(m, 0, "sdist", &nzt_i_sdist, 1);
	state_save_register_int(m, 0, "r2dxbank", &m_r2dxbank);
	state_save_register_int(m, 0, "r2dxgameselect", &m_r2dxgameselect);
	state_save_register_int(m, 0, "okibank", &r2dx_okibank);

	state_save_register_func_postload(r2_state_postload);
}

/* -----------------------------------------------------------------------
 * ROMs: names, contents and layout as in current MAME. Raiden II and Zero
 * Team load the program twice: the low copy is mapped as RAM by this core's
 * memory system, the high copy keeps the banked pages intact.
 * ----------------------------------------------------------------------- */

ROM_START( raiden2 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg0.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("prg1.u0212",   0x000001, 0x80000, CRC(4609b5f2) SHA1(272d2aa75b8ea4d133daddf42c4fc9089093df2e) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "snd.u1110",  0x000000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2g )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg0.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) ) /* rom1 - same code base as raiden2 */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("prg1g.u0212",  0x000001, 0x80000, CRC(41001d2e) SHA1(06bece44c081ecbb3b8dac5c515e30c5a5ffc1bf) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "snd.u1110",  0x000000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 - sldh w/raiden2u */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2gb ) /* original board with serial number on sticker on u0212 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg0.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("prg1.u0212",   0x000001, 0x80000, CRC(4b9e3024) SHA1(10f0384cff6acac5823c77277b3c75e83d34589c) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "snd.u1110",  0x000000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2hk )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg0.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) ) /* rom1 - same code base as raiden2 */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("rom2e.u0212",  0x000001, 0x80000, CRC(458d619c) SHA1(842bf0eeb5d192a6b188f4560793db8dad697683) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu5.u1110",  0x000000, 0x08000, CRC(8f130589) SHA1(e58c8beaf9f27f063ffbcb0ab4600123c25ce6f3) ) /* sldh w/raiden2u */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2j )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg0.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) ) /* rom1 - same code base as raiden2 */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("rom2j.u0212",  0x000001, 0x80000, CRC(e4e4fb4c) SHA1(7ccf33fe9a1cddf0c7e80d7ed66d615a828b3bb9) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu5.u1110",  0x000000, 0x08000, CRC(8f130589) SHA1(e58c8beaf9f27f063ffbcb0ab4600123c25ce6f3) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 - sldh w/raiden2u */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2sw ) /* original board with serial # 0008307 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("seibu_1.u0211",   0x000000, 0x80000, CRC(09475ec4) SHA1(05027f2d8f9e11fcbd485659eda68ada286dae32) ) /* rom1 - same code base as raiden2 - sldh w/raiden2eu */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("seibu_2.u0212",   0x000001, 0x80000, CRC(59abc2ec) SHA1(45f2dbd2dd46f5da07dae0dc486772f8e61f4c43) ) /* sldh w/raiden2eu */
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(c2028ba2) SHA1(f6a9322b669ff82dea6ecf52ad3bd5d0901cce1b) ) /* 99.993896% match */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2u )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("1.u0211",  0x000000, 0x80000, CRC(b16df955) SHA1(9b7fd85cf2f2c9fea657f3c38abafa93673b3933) ) /* unique unknown code base */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("2.u0212",  0x000001, 0x80000, CRC(2a14b112) SHA1(84cd9891b5be0b71b2bae3487ad38bed3045305e) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu5.u1110", 0x000000, 0x08000, CRC(6d362472) SHA1(a362e500bb9492affde1f7a4da7e08dd16e755df) ) /* sldh w/raiden2hk */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) ) /* sldh w/raiden2g */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) ) /* sldh w/raiden2j */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2f ) /* original board with serial # 12476 that matches raiden2nl set except the region and Audio CPU */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("1_u0211.bin",       0x000000, 0x80000, CRC(53be3dd0) SHA1(304d118423e4085eea3b883bd625d90d21bb2054) )   /* same code base as raiden2nl & raiden2es */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("seibu2_u0212.bin",  0x000001, 0x80000, CRC(8dcd8a8d) SHA1(be0681d5867d8b4f5fb78946a896d89827a71e8e) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu5_u1110.bin",  0x000000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302) )   /* == raiden2 */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "7_u0724.bin", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "6_u1017.bin", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2nl )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("1_u0211.bin",  0x000000, 0x80000, CRC(53be3dd0) SHA1(304d118423e4085eea3b883bd625d90d21bb2054) )   /* same code base as raiden2f & raiden2es */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("2_u0212.bin",  0x000001, 0x80000, CRC(88829c08) SHA1(ecdfbafeeffcd009bbc4cf5bf797bcd4b5bfcf50) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5_u1110.bin",  0x000000, 0x08000, CRC(8f130589) SHA1(e58c8beaf9f27f063ffbcb0ab4600123c25ce6f3) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "7_u0724.bin", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "6_u1017.bin", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2es )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("1_u0211.bin",  0x000000, 0x80000, CRC(53be3dd0) SHA1(304d118423e4085eea3b883bd625d90d21bb2054) )   /* same code base as raiden2f & raiden2nl */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("2_u0212.rom",  0x000001, 0x80000, CRC(9dbec61c) SHA1(59ed06d9f97d93486dec2c0d8c0f42f59fb19db0) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5_u1110.bin",  0x000000, 0x08000, CRC(8f130589) SHA1(e58c8beaf9f27f063ffbcb0ab4600123c25ce6f3) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "7_u0724.bin", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "6_u1017.bin", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2i )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("seibu1.u0211",   0x000000, 0x80000, CRC(c1fc70f5) SHA1(a054f5ae9583972c406d9cf871340d5e072d71a3) ) /* Italian set  - unique unknown code base */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("seibu2.u0212",   0x000001, 0x80000, CRC(28d5365f) SHA1(21efe29c2d373229c2ff302d86e59c2c94fa6d03) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu5.c.u1110",  0x000000, 0x08000, CRC(5db9f922) SHA1(8257aab98657fe44df19d2a48d85fcf65b3d98c6) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2au )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("1.u0211",   0x000000, 0x80000, CRC(c1fc70f5) SHA1(a054f5ae9583972c406d9cf871340d5e072d71a3) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("2.u0212",   0x000001, 0x80000, CRC(396410f9) SHA1(a3f737ae1fb6229388d3ebec93e880218d09fed5) ) /* Australian set */
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF )
	ROM_LOAD( "5.u1110",  0x000000, 0x08000, CRC(c2028ba2) SHA1(f6a9322b669ff82dea6ecf52ad3bd5d0901cce1b) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "voice_2.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2e )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("r2_prg_0.u0211",   0x000000, 0x80000, CRC(2abc848c) SHA1(1df4276d0074fcf1267757fa0b525a980a520f3d) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("r2_prg_1.u0212",   0x000001, 0x80000, CRC(509ade43) SHA1(7cdee7bb00a6a1c7899d10b96385d54c261f6f5a) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "r2_snd.u1110", 0x000000, 0x08000, CRC(6bad0a3e) SHA1(eb7ae42353e1984cd60b569c26cdbc3b025a7da6) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "r2_fx0.u0724",   0x000000, 0x020000, CRC(c709bdf6) SHA1(0468d90412b7590d67eaadc0a5e3537cd5e73943) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "r2_voi1.u1017", 0x00000, 0x40000, CRC(488d050f) SHA1(fde2fd64fea6bc39e1a42885d21d362bc6be2ac2) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2eub )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("r2_prg_0.u0211",   0x000000, 0x80000, CRC(2abc848c) SHA1(1df4276d0074fcf1267757fa0b525a980a520f3d) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("r2_prg_1.u0212",   0x000001, 0x80000, CRC(56511ca8) SHA1(1073f9a5dab185b161306d1f0db44ae84865294b) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "r2_snd.u1110", 0x000000, 0x08000, CRC(6d362472) SHA1(a362e500bb9492affde1f7a4da7e08dd16e755df) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "r2.7.u0724",   0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "r2.6.u1017", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2eua ) /* sort of a mixture of raiden2e easy set with voice ROM of raiden2ea and 2f and a unique sound ROM */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("seibu__1.27c020j.u1210",   0x000000, 0x40000, CRC(ed1514e3) SHA1(296125bfe3c4f3033f7aa319dd8554bc978c4a00) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("seibu__2.27c2001.u1211",   0x000001, 0x40000, CRC(bb6ecf2a) SHA1(d4f628e9d0ed2897654f05a8a2541e1ed3faf8dd) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("seibu__3.27c2001.u129",    0x000002, 0x40000, CRC(6a01d52c) SHA1(983b914592ab9d9c058bebb5bccf5c882e2b82de) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("seibu__4.27c2001.u1212",   0x000003, 0x40000, CRC(e54bfa37) SHA1(4fabb23503fd9245a10cded15a6880415ca5ffd7) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu__5.27c512.u1110", 0x000000, 0x08000, CRC(6d362472) SHA1(a362e500bb9492affde1f7a4da7e08dd16e755df) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu__7.fx0.27c210.u0724", 0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu__6.voice1.23c020.u1017", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2eg ) /* this is the same code revision as raiden2eua but a german region */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("raiden_2_1.u1210",   0x000000, 0x40000, CRC(ed1514e3) SHA1(296125bfe3c4f3033f7aa319dd8554bc978c4a00) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("raiden_2_2.u1211",   0x000001, 0x40000, CRC(bb6ecf2a) SHA1(d4f628e9d0ed2897654f05a8a2541e1ed3faf8dd) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("raiden_2_3.u129",    0x000002, 0x40000, CRC(6a01d52c) SHA1(983b914592ab9d9c058bebb5bccf5c882e2b82de) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("raiden_2_4.u1212",   0x000003, 0x40000, CRC(81273f33) SHA1(074cedf44cc5286649cc101bce0b48d40234e472) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "raiden_2_5.bin", 0x000000, 0x08000, CRC(6d362472) SHA1(a362e500bb9492affde1f7a4da7e08dd16e755df) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "raiden_2_7.bin", 0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_6.bin", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2eup )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("prg0 11-16.u1210",   0x000000, 0x40000, CRC(0a68a400) SHA1(7b3baae086ea9604af29eabde358da358d43591f) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("prg1 11-16.u1211",   0x000001, 0x40000, CRC(45a01ebe) SHA1(8b114198130ead8a70cc64b195fba5a3507ff6cb) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("prg2 11-16.u129",    0x000002, 0x40000, CRC(9a6e33d7) SHA1(fadaec486cd4163d4b8f41aac171baaac3505e30) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("prg3a 11-16.u1212",  0x000003, 0x40000, CRC(42ce511b) SHA1(59f199cb52cb315ddea0373b978541b8efeb57a3) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "sound 11-12.bin", 0x000000, 0x08000, CRC(4fac206c) SHA1(47b78b2efe88729df07033a8674ba203cdf2e44c) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "fix.bin", 0x000000, 0x020000, CRC(6a6fa0de) SHA1(4e2ba9e84f5f5684b480e6d7f86bfc08ac9f5337) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "pcm 11-11.bin", 0x00000, 0x40000, CRC(c541c729) SHA1(23f0b454d8d813bfcdb831ee5a8547178892231f) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2ea )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("r2.1.u0211",  0x000000, 0x80000, CRC(d7041be4) SHA1(3cf97132fba6f7b00c9059265f4e9f0bf1505b71) )
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("r2.2.u0212",  0x000001, 0x80000, CRC(bf7577ec) SHA1(98576af78760b8aef1ef3efe1ba963977c89d225) )
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "r2.5.u1110", 0x000000, 0x08000, CRC(f5f835af) SHA1(5be82ebc582d0da919e9ae1b9e64528bb295efc7) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "r2.7.u0724", 0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "r2.6.u1017", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2eu ) /* same as raiden2ea, different region */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("seibu_1.u0211",  0x000000, 0x80000, CRC(d7041be4) SHA1(3cf97132fba6f7b00c9059265f4e9f0bf1505b71) ) /* sldh w/raiden2sw */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("seibu_2.u0212",  0x000001, 0x80000, CRC(beb71ddb) SHA1(471399ead1cdc27ac2a1139f9616f828efd14626) ) /* sldh w/raiden2sw */
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "r2.5.u1110", 0x000000, 0x08000, CRC(f5f835af) SHA1(5be82ebc582d0da919e9ae1b9e64528bb295efc7) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "r2.7.u0724", 0x000000, 0x020000, CRC(c7aa4d00) SHA1(9ad99d3891598c1ea3f12318400ee67666da56dd) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "r2.6.u1017", 0x00000, 0x40000, CRC(fab9f8e4) SHA1(b1eff154c4f766b2d272ac6a57f8d54c9e39e3bb) )

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2k )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("k1.u0211",   0x000000, 0x80000, CRC(1fcc08cf) SHA1(bff7076ced189120166217d71e2762bb98aad7c8) ) /* hand-written */
	ROM_RELOAD(                      0x100000, 0x80000 )
	ROM_LOAD16_BYTE("k2.u0212",   0x000001, 0x80000, CRC(59a744ca) SHA1(5fdd7dd4049f944df23371e2e2d3133b10c66ab8) ) /* hand-written */
	ROM_RELOAD(                      0x100001, 0x80000 )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "snd.u1110",  0x000000, 0x08000, CRC(f51a28f9) SHA1(7ae2e2ba0c8159a544a8fd2bb0c2c694ba849302) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu7.u0724", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu6.u1017", 0x00000, 0x40000, CRC(fb0fca23) SHA1(4b2217b121a66c5ab6015537609cf908ffedaf86) ) /* PCB silkscreened VOICE1 */

	/* Common Raiden II PALs below */
	ROM_REGION( 0x10000, REGION_USER8, ROMREGION_DISPOSE )    /* PALS */
	ROM_LOAD( "jj4b02__ami18cv8-15.u0342",   0x0000, 0x155, CRC(057a9cdc) SHA1(8b46f6673ddf11efbc3394ae423ec89d4a1283bf) )
	ROM_LOAD( "jj4b01__mmipal16l8bcn.u0341", 0x0000, 0x117, CRC(20931f21) SHA1(95ce9cfbfb280dfc6a326e378684eff3c6f54701) )

	/* Common Raiden II soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313", 0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "raiden_2_seibu_bg-1.u075",  0x000000, 0x200000, CRC(e61ad38e) SHA1(63b06cd38db946ad3fc5c1482dc863ef80b58fec) )
	ROM_LOAD( "raiden_2_seibu_bg-2.u0714", 0x200000, 0x200000, CRC(a694a4bb) SHA1(39c2614d0effc899fe58f735604283097769df77) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-3.u0837", 0x400000, 0x200000, CRC(897a0322) SHA1(abb2737a2446da5b364fc2d96524b43d808f4126) )
	ROM_LOAD32_WORD( "raiden_2_seibu_obj-4.u0836", 0x400002, 0x200000, CRC(b676e188) SHA1(19cc838f1ccf9c4203cd0e5365e5d99ff3a4ff0f) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) )
ROM_END

ROM_START( raiden2dx ) /* this set is very weird, it's Raiden II on a Raiden DX board, I'm assuming for now that it uses Raiden DX graphics, but could be wrong. */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("u1210.bin",      0x000000, 0x80000, CRC(413241e0) SHA1(50fa501db91412baea474a8faf8ad483f3a119c7) )
	ROM_LOAD32_BYTE("prg1_u1211.bin", 0x000001, 0x80000, CRC(93491f56) SHA1(2239980fb7267906e4c3985703c2dc2932b23705) )
	ROM_LOAD32_BYTE("u129.bin",       0x000002, 0x80000, CRC(e0932b6c) SHA1(04f1ca885d220e802023042438f63e40e4106696) )
	ROM_LOAD32_BYTE("u1212.bin",      0x000003, 0x80000, CRC(505423f4) SHA1(d8e65580deec05dd84c4cf3074cb690e3764c625) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "u1110.bin",  0x000000, 0x08000,  CRC(b8ad8fe7) SHA1(290896f811f717ef6e3ec2152d4db98a9fe9b310) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	/* ROM_LOAD( "fx0_u0724.bin",    0x000000,   0x020000,   CRC(ded3c718) SHA1(c722ec45cd1b2dab23aac14e9113e0e9697830d3) ) // bad dump */
	ROM_LOAD( "7_u0724.bin", 0x000000, 0x020000, CRC(c9ec9469) SHA1(a29f480a1bee073be7a177096ef58e1887a5af24) ) /* PCB silkscreened FX0 */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "dx_6.3b",   0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX(!) soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.6s",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back1.1s",   0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back2.2s",   0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj1",        0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "obj2",        0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj3.4k",  0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj4.6k",  0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "dx_pcm.3a", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Shared with original Raiden 2 */
ROM_END

ROM_START( raidendx )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1d.4n",   0x000000, 0x80000, CRC(14d725fc) SHA1(f12806f64f069fdc4ee29b309a32f7ca00b36f93) )
	ROM_LOAD32_BYTE("2d.4p",   0x000001, 0x80000, CRC(5e7e45cb) SHA1(94eff893b5335c522f1c063c3175b9bac87b0a25) )
	ROM_LOAD32_BYTE("3d.6n",   0x000002, 0x80000, CRC(f0a47e67) SHA1(8cbd21993077b2e01295db6e343cae9e0e4bfefe) )
	ROM_LOAD32_BYTE("4d.6p",   0x000003, 0x80000, CRC(2a2003e8) SHA1(f239b351759babe4683d16e745a5ac2f3c2ab06b) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxg )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1d.u1210", 0x000000, 0x80000, CRC(14d725fc) SHA1(f12806f64f069fdc4ee29b309a32f7ca00b36f93) )
	ROM_LOAD32_BYTE("2d.u1211", 0x000001, 0x80000, CRC(5e7e45cb) SHA1(94eff893b5335c522f1c063c3175b9bac87b0a25) )
	ROM_LOAD32_BYTE("3d.u129",  0x000002, 0x80000, CRC(f0a47e67) SHA1(8cbd21993077b2e01295db6e343cae9e0e4bfefe) )
	ROM_LOAD32_BYTE("4d.u1212", 0x000003, 0x80000, CRC(6bde6edc) SHA1(c3565a55b858c10659fd9b93b1cd92bc39e6446d) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxpt )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("seibu_1d.u1210", 0x000000, 0x80000, CRC(14d725fc) SHA1(f12806f64f069fdc4ee29b309a32f7ca00b36f93) )
	ROM_LOAD32_BYTE("seibu_2d.u1211", 0x000001, 0x80000, CRC(5e7e45cb) SHA1(94eff893b5335c522f1c063c3175b9bac87b0a25) )
	ROM_LOAD32_BYTE("seibu_3d.u129",  0x000002, 0x80000, CRC(f0a47e67) SHA1(8cbd21993077b2e01295db6e343cae9e0e4bfefe) )
	ROM_LOAD32_BYTE("seibu_4d.u1212", 0x000003, 0x80000, CRC(e4fa53ac) SHA1(640257420beba6acaeaf687fde34dd20aef7c41a) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxa1 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("dx_1h.4n",   0x000000, 0x80000, BAD_DUMP CRC(7624c36b) SHA1(84c17f2988031210d06536710e1eac558f4290a1) ) /* bad */
	ROM_LOAD32_BYTE("dx_2h.4p",   0x000001, 0x80000, CRC(4940fdf3) SHA1(c87e307ed7191802583bee443c7c8e4f4e33db25) )
	ROM_LOAD32_BYTE("dx_3h.6n",   0x000002, 0x80000, CRC(6c495bcf) SHA1(fb6153ecc443dabc829dda6f8d11234ad48de88a) )
	ROM_LOAD32_BYTE("dx_4h.6k",   0x000003, 0x80000, CRC(9ed6335f) SHA1(66975204b120915f23258a431e19dbc017afd912) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxa2 )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1d.bin",   0x000000, 0x80000, CRC(22b155ae) SHA1(388151e2c8fb301bd5bc66a974e9fe16816ae0bc) )
	ROM_LOAD32_BYTE("2d.bin",   0x000001, 0x80000, CRC(2be98ca8) SHA1(491e990405b0ad3de45bdbcc2453af9215ae19c8) )
	ROM_LOAD32_BYTE("3d.bin",   0x000002, 0x80000, CRC(b4785576) SHA1(aa5eee7b0c635c6d18a7fc1e037bf570a677dd90) )
	ROM_LOAD32_BYTE("4d.bin",   0x000003, 0x80000, CRC(5a77f7b4) SHA1(aa757e6308893ca63963170c5b1743de7c7ab034) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxk )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("rdxj_1.bin",   0x000000, 0x80000, CRC(b5b32885) SHA1(fb3c592b2436d347103c17bd765176062be95fa2) )
	ROM_LOAD32_BYTE("rdxj_2.bin",   0x000001, 0x80000, CRC(7efd581d) SHA1(4609a0d8afb3d62a38b461089295efed47beea91) )
	ROM_LOAD32_BYTE("rdxj_3.bin",   0x000002, 0x80000, CRC(55ec0e1d) SHA1(6be7f268df51311a817c1c329a578b38abb659ae) )
	ROM_LOAD32_BYTE("rdxj_4.bin",   0x000003, 0x80000, CRC(f8fb31b4) SHA1(b72fd7cbbebcf3d1b2253c309fcfa60674776467) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxu )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1a.u1210", 0x000000, 0x80000, CRC(53e63194) SHA1(a957330e14649cf46ad27fb99c460576c59e60b1) )
	ROM_LOAD32_BYTE("2a.u1211", 0x000001, 0x80000, CRC(ec8d1647) SHA1(5ceae132c6c09d6bb8565e9141ee1170bbdfd5fc) )
	ROM_LOAD32_BYTE("3a.u129",  0x000002, 0x80000, CRC(7dbfd73d) SHA1(43cb1dbc3ccbded64fc300c262d1fd528e0391a2) )
	ROM_LOAD32_BYTE("4a.u1212", 0x000003, 0x80000, CRC(cb41a459) SHA1(532f0ed00a5b50a7537e5f48884d632aa5b92fb0) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxnl )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("u1210_4n.bin", 0x000000, 0x80000, CRC(c589019a) SHA1(9bdd7f7d0bca16d67ba234d8a1fed5d2c8ab7191) )
	ROM_LOAD32_BYTE("u1211_4p.bin", 0x000001, 0x80000, CRC(b2222254) SHA1(b0e41d88111a96f0c0fb11b20ea99f436e8d493d) )
	ROM_LOAD32_BYTE("u129_6n.bin",  0x000002, 0x80000, CRC(60f04634) SHA1(50f1b721a017d879838d920cf5d5355aa024e09b) )
	ROM_LOAD32_BYTE("u1212_6p.bin", 0x000003, 0x80000, CRC(21ec37cc) SHA1(6da629e2bb5bd4c2192156af017148e99e274544) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxj )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("rdxj_1.u1211", 0x000000, 0x80000, CRC(5af382e1) SHA1(a11fc181da322f484815f55a510ce7e6c7df2d60) )
	ROM_LOAD32_BYTE("rdxj_2.u0212", 0x000001, 0x80000, CRC(899966fc) SHA1(0f91c2b05a44afb4c4b74e115a8fa530fb6d6414) )
	ROM_LOAD32_BYTE("rdxj_3.u129",  0x000002, 0x80000, CRC(e7f08013) SHA1(1f99672d8fdbda847c6552da210c417b21ca78ac) )
	ROM_LOAD32_BYTE("rdxj_4.u1212", 0x000003, 0x80000, CRC(78037e1f) SHA1(8d9c4188ca808e670e330e70e906bb1d27e36492) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "rdxj_7.u0724", 0x000000, 0x020000, CRC(ec31fa10) SHA1(e39c9d95699dbeb21e3661d863eee503c9011bbc) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxja )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1.bin", 0x000000, 0x80000, CRC(247e21c7) SHA1(69e9a084407f57e8433f832a592cfbba2757631f) )
	ROM_LOAD32_BYTE("2.bin", 0x000001, 0x80000, CRC(f2e9855a) SHA1(e27ac94cdb4ce8e8a98b342fbaf0fde11d9ab9ef) )
	ROM_LOAD32_BYTE("3.bin", 0x000002, 0x80000, CRC(fbab727f) SHA1(5415ff87dd967e52b5cf7d754c046223cfa1147b) )
	ROM_LOAD32_BYTE("4.bin", 0x000003, 0x80000, CRC(a08d5838) SHA1(ec07770503eefa5f22d30f40e3df2a02813908e4) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "rdxj_7.u0724", 0x000000, 0x020000, CRC(ec31fa10) SHA1(e39c9d95699dbeb21e3661d863eee503c9011bbc) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( raidendxch )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("rdxc_1.u1210", 0x000000, 0x80000, CRC(2154c6ae) SHA1(dc794f8ddbd8a6267db37fe4e3ed44e06e9b84b7) )
	ROM_LOAD32_BYTE("rdxc_2.u1211", 0x000001, 0x80000, CRC(73bb74b7) SHA1(2f197adbe89d96c9e75054c568c380fdd2e80162))
	ROM_LOAD32_BYTE("rdxc_3.u129",  0x000002, 0x80000, CRC(50f0a6aa) SHA1(68579f8e73fe06b458368ac9cac0b33370cf3b4e))
	ROM_LOAD32_BYTE("rdxc_4.u1212", 0x000003, 0x80000, CRC(00071e70) SHA1(8a03ea0e650936e48cdd21ff84132742649920fe) )

	/* no other ROMs present with this set, so the ones below could be wrong */
	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5.u1110",  0x000000, 0x08000, CRC(8c46857a) SHA1(8b269cb20adf960ba4eb594d8add7739dbc9a837) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "seibu_7.u0724",    0x000000,   0x020000,   CRC(c73986d4) SHA1(d29345077753bda53560dedc95dd23f329e521d9) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )   /* ADPCM samples */
	ROM_LOAD( "seibu_6.u1017", 0x00000, 0x40000, CRC(9a9196da) SHA1(3d1ee67fb0d40a231ce04d10718f07ffb76db455) )

	/* Common Raiden DX soldered mask ROMs below */
	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) ) /* Shared with original Raiden 2 */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "dx_back-1.u075",  0x000000, 0x200000, CRC(90970355) SHA1(d71d57cd550a800f583550365102adb7b1b779fc) )
	ROM_LOAD( "dx_back-2.u0714", 0x200000, 0x200000, CRC(5799af3e) SHA1(85d6532abd769da77bcba70bd2e77915af40f987) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "raiden_2_obj-1.u0811", 0x000000, 0x200000, CRC(ff08ef0b) SHA1(a1858430e8171ca8bab785457ef60e151b5e5cf1) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "raiden_2_obj-2.u082",  0x000002, 0x200000, CRC(638eb771) SHA1(9774cc070e71668d7d1d20795502dccd21ca557b) ) /* Shared with original Raiden 2 */
	ROM_LOAD32_WORD( "dx_obj-3.u0837",       0x400000, 0x200000, CRC(ba381227) SHA1(dfc4d659aca1722a981fa56a31afabe66f444d5d) )
	ROM_LOAD32_WORD( "dx_obj-4.u0836",       0x400002, 0x200000, CRC(65e50d19) SHA1(c46147b4132abce7314b46bf419ce4773e024b05) )

	ROM_REGION( 0x100000, REGION_SOUND2, 0 )   /* ADPCM samples */
	ROM_LOAD( "raiden_2_pcm.u1018", 0x00000, 0x40000, CRC(8cf0d17e) SHA1(0fbe0b1e1ca5360c7c8329331408e3d799b4714c) ) /* Mask ROM - Shared with original Raiden 2 */
ROM_END

ROM_START( zeroteam ) /* Fabtek, US licensee, displays 'USA' under zero team logo, board had serial 'Seibu Kaihatsu No. 0001468' on it, as well as AAMA 0458657 */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("seibu__1.u024.5k",   0x000000, 0x40000, CRC(25aa5ba4) SHA1(40e6047620fbd195c87ac3763569af099096eff9) ) /* alternate label "1" */
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("seibu__3.u023.6k",   0x000002, 0x40000, CRC(ec79a12b) SHA1(515026a2fca92555284ac49818499af7395783d3) ) /* alternate label "3" */
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("seibu__2.u025.6l",   0x000001, 0x40000, CRC(54f3d359) SHA1(869744185746d55c60d2f48eabe384a8499e00fd) ) /* alternate label "2" */
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("seibu__4.u026.5l",   0x000003, 0x40000, CRC(a017b8d0) SHA1(4a93ff1ab18f4b61c7ef580995f64840c19ce6b9) ) /* alternate label "4" */
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu__5.u1110.5b",  0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* // alternate label "5" */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "seibu__7.u072.5s",    0x000000,   0x010000,   CRC(9f6aa0f0) SHA1(1caad7092c07723d12a07aa363ae2aa69cb6be0d) ) /* alternate label "7" */
	ROM_LOAD16_BYTE( "seibu__8.u077.5r",    0x000001,   0x010000,   CRC(68f7dddc) SHA1(6938fa974c6ef028751982fdabd6a3820b0d30a8) ) /* alternate label "8" */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "seibu__6.u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* alternate label "6" */

ROM_END

ROM_START( zeroteama ) /* No licensee, original japan? */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1.u024.5k",   0x000000, 0x40000, CRC(bd7b3f3a) SHA1(896413901a429d0efa3290f61920063c81730e9b) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("3.u023.6k",   0x000002, 0x40000, CRC(19e02822) SHA1(36c9b887eaa9b9b67d65c55e8f7eefd08fe0be15) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("2.u025.6l",   0x000001, 0x40000, CRC(0580b7e8) SHA1(d4416264aa5acdaa781ebcf51f128b3e665cc903) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("4.u026.5l",   0x000003, 0x40000, CRC(cc666385) SHA1(23a8878315b6009dcc1f27e49572e5be29f6a1a6) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5.a.u1110.5b",  0x000000, 0x08000, CRC(efc484ca) SHA1(c34b8e3e7f4c2967bc6414348993478ed637d338) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "7.a.u072.5s", 0x000000,   0x010000, CRC(eb10467f) SHA1(fc7d576dc41bc878ff20f0370e669e19d54fcefb) )
	ROM_LOAD16_BYTE( "8.a.u077.5r", 0x000001,   0x010000, CRC(a0b2a09a) SHA1(9b1f6c732000b84b1ad635f332ebead5d65cc491) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "6.u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* 6.bin */

ROM_END

ROM_START( zeroteamb ) /* No licensee, later japan? */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1b.u024.5k",   0x000000, 0x40000, CRC(157743d0) SHA1(f9c84c9025319f76807ef0e79f1ee1599f915b45) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("3b.u023.6k",   0x000002, 0x40000, CRC(fea7e4e8) SHA1(08c4bdff82362ae4bcf86fa56fcfc384bbf82b71) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("2b.u025.6l",   0x000001, 0x40000, CRC(21d68f62) SHA1(8aa85b38e8f36057ef6c7dce5a2878958ce93ce8) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("4b.u026.5l",   0x000003, 0x40000, CRC(ce8fe6c2) SHA1(69627867c7866e43e771ab85014553117044d18d) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5.u1110.5b",  0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* 5.5c */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "7.u072.5s",   0x000000,   0x010000,   CRC(9f6aa0f0) SHA1(1caad7092c07723d12a07aa363ae2aa69cb6be0d) )
	ROM_LOAD16_BYTE( "8.u077.5r",   0x000001,   0x010000,   CRC(68f7dddc) SHA1(6938fa974c6ef028751982fdabd6a3820b0d30a8) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "6.u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* 6.4a */

ROM_END

ROM_START( zeroteamc ) /* Liang Hwa, Taiwan licensee, no special word under logo on title */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("b1.u024.5k",   0x000000, 0x40000, CRC(528de3b9) SHA1(9ca8cdc0212f2540e852d20ab4c04f68b967d024) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("b3.u023.6k",   0x000002, 0x40000, CRC(3688739a) SHA1(f98f461fb8e7804b3b4020a5e3762d36d6458a62) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("b2.u025.6l",   0x000001, 0x40000, CRC(5176015e) SHA1(6b372564b2f1b1f56cae0c98f4ca588b784bfa3d) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("b4.u026.5l",   0x000003, 0x40000, CRC(c79925cb) SHA1(aaff9f626ec61bc0ff038ebd722fe361dccc49fb) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5.c.u1110.5b",  0x000000, 0x08000, CRC(efc484ca) SHA1(c34b8e3e7f4c2967bc6414348993478ed637d338) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "b7.u072.5s",  0x000000,   0x010000, CRC(30ec0241) SHA1(a0d0be9458bf97cb9764fb85c988bb816710475e) )
	ROM_LOAD16_BYTE( "b8.u077.5r",  0x000001,   0x010000, CRC(e18b3a75) SHA1(3d52bba8d47d0d9108ee79014fd64d6e856a3fde) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "6.c.u105.4a", 0x00000, 0x40000,  CRC(b4a6e899) SHA1(175ab656db3c3258ff10eede89890f62435d2298) )

ROM_END

ROM_START( zeroteamd ) /* Dream Soft, Korea licensee, no special word under logo on title; board had serial 'no 1041' on it. */
	/* this is weird, on other zt sets the ROM order is 1 3 2 4, but this one is 1 3 4 2. blame seibu or whoever marked the ROMs, which were labeled in pen */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1.d.u024.5k",   0x000000, 0x40000, CRC(6cc279be) SHA1(63143ba3105d24d133e60ffdb3edc2ceb2d5dc5b) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("3.d.u023.6k",   0x000002, 0x40000, CRC(0212400d) SHA1(28f77b5fddb9d724b735c3ff2255bd518b166e67) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("4.d.u025.6l",   0x000001, 0x40000, CRC(08813ebb) SHA1(454779cec2fd0e71b72f7161e7d9334893ee42de) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("2.d.u026.5l",   0x000003, 0x40000, CRC(9236129d) SHA1(8561ab62e3593cd9353d9ffddedbdb77e9ae2c45) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "512kb.u1110.5b",  0x000000, 0x08000, CRC(efc484ca) SHA1(c34b8e3e7f4c2967bc6414348993478ed637d338) ) /* this is a soldered mask ROM on this pcb version! the contents match the taiwan version EPROM; the mask ROM has no label */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "512kb.u072.5s",   0x000000,   0x010000, CRC(30ec0241) SHA1(a0d0be9458bf97cb9764fb85c988bb816710475e) ) /* this is a soldered mask ROM on this pcb version! the contents match the taiwan version EPROM; the mask ROM has no label */
	ROM_LOAD16_BYTE( "512kb.u077.5r",   0x000001,   0x010000, CRC(e18b3a75) SHA1(3d52bba8d47d0d9108ee79014fd64d6e856a3fde) ) /* this is a soldered mask ROM on this pcb version! the contents match the taiwan version EPROM; the mask ROM has no label */

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "8.u105.4a", 0x00000, 0x40000,  CRC(b4a6e899) SHA1(175ab656db3c3258ff10eede89890f62435d2298) ) /* same ROM as '6' labeled one in zeroteamc above but has '8' written on label in pen */

ROM_END

ROM_START( zeroteame ) /* shares chars ROMs with zeroteama */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("seibu_1_u024.5k",   0x000000, 0x40000, CRC(76e69af5) SHA1(c5a4b2491cee3f4f9694e454f3828fda33fff3ab) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("seibu_3_u023.6k",   0x000002, 0x40000, CRC(4a904880) SHA1(4a85a62446ea11d57369dfd65f143475063abc31) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("seibu_2_u025.6l",   0x000001, 0x40000, CRC(b97ab448) SHA1(82d9e7aa6b69c214ad82e5ff30a049cecab607c0) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("seibu_4_u026.5l",   0x000003, 0x40000, CRC(1d43b9c1) SHA1(b1144d49f8fddf416fe2e3eae4040b369eb212cf) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_5_u1110.5b",  0x000000, 0x08000, CRC(efc484ca) SHA1(c34b8e3e7f4c2967bc6414348993478ed637d338) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "seibu_7_u072.5s", 0x000000,   0x010000, CRC(eb10467f) SHA1(fc7d576dc41bc878ff20f0370e669e19d54fcefb) )
	ROM_LOAD16_BYTE( "seibu_8_u077.5r", 0x000001,   0x010000, CRC(a0b2a09a) SHA1(9b1f6c732000b84b1ad635f332ebead5d65cc491) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "seibu_6_u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* 6.bin */

ROM_END

ROM_START( zeroteams ) /* No license, displays 'Selection' under logo */
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("1_sel.bin",   0x000000, 0x40000, CRC(d99d6273) SHA1(21dccd5d71c720b8364406835812b3c9defaff6c) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("3_sel.bin",   0x000002, 0x40000, CRC(0a9fe0b1) SHA1(3588fe19788f77d07e9b5ab8182b94362ffd0024) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("2_sel.bin",   0x000001, 0x40000, CRC(4e114e74) SHA1(fcccbb68c6b7ffe8d109ed3a1ec9120d338398f9) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("4_sel.bin",   0x000003, 0x40000, CRC(0df8ba94) SHA1(e07dce6cf3c3cfe1ea3b7f01e18833c1da5ed1dc) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5_sel.bin",  0x000000, 0x08000, CRC(ed91046c) SHA1(de815c999aeeb814d3f091d5a9ac34ea9a388ddb) )
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "7.u072.5s",   0x000000,   0x010000,   CRC(9f6aa0f0) SHA1(1caad7092c07723d12a07aa363ae2aa69cb6be0d) )
	ROM_LOAD16_BYTE( "8.u077.5r",   0x000001,   0x010000,   CRC(68f7dddc) SHA1(6938fa974c6ef028751982fdabd6a3820b0d30a8) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "6.u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* 6.bin */

ROM_END

ROM_START( zeroteamsr )
	ROM_REGION( 0x200000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD32_BYTE("zteam1.u24",   0x000000, 0x40000, CRC(c531e009) SHA1(731881fca3dc0a8269ecdd295ba7119d93c892e7) )
	ROM_RELOAD(                      0x100000, 0x40000 )
	ROM_LOAD32_BYTE("zteam3.u23",   0x000002, 0x40000, CRC(1f988808) SHA1(b1fcb8c96e57c4942bc032d42408d7289c6a3681) )
	ROM_RELOAD(                      0x100002, 0x40000 )
	ROM_LOAD32_BYTE("zteam2.u25",   0x000001, 0x40000, CRC(b7234b93) SHA1(35bc093e8ad4bce1d2130a392ed1b9487a5642a1) )
	ROM_RELOAD(                      0x100001, 0x40000 )
	ROM_LOAD32_BYTE("zteam4.u26",   0x000003, 0x40000, CRC(c2d26708) SHA1(d65191b40f5dd7cdbbc004e2de10134db6092fd1) )
	ROM_RELOAD(                      0x100003, 0x40000 )

	ROM_REGION( 0x40000, REGION_USER1, 0 )   /* COPX */
	ROM_LOAD( "copx-d2.u0313.6n",   0x00000, 0x40000, CRC(a6732ff9) SHA1(c4856ec77869d9098da24b1bb3d7d58bb74b4cda) )

	ROM_REGION( 0x20000, REGION_CPU2, ROMREGION_ERASEFF ) /* 64k code for sound Z80 */
	ROM_LOAD( "5.5c",  0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* 5.5c */
	ROM_CONTINUE(0x10000,0x8000)
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "7.u072.5s",   0x000000,   0x010000,   CRC(9f6aa0f0) SHA1(1caad7092c07723d12a07aa363ae2aa69cb6be0d) )
	ROM_LOAD16_BYTE( "8.u077.5r",   0x000001,   0x010000,   CRC(68f7dddc) SHA1(6938fa974c6ef028751982fdabd6a3820b0d30a8) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "musha_back-1.u075.4s",   0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) )
	ROM_LOAD( "musha_back-2.u0714.2s",   0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) )

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) (diff encrypt to raiden2? ) */
	ROM_LOAD32_WORD( "musha_obj-1.u0811.6f",  0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) )
	ROM_LOAD32_WORD( "musha_obj-2.u082.5f",  0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 )    /* ADPCM samples */
	ROM_LOAD( "6.u105.4a", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* 6.4a */

ROM_END

ROM_START( r2dx_v33 )
	ROM_REGION( 0x400000, REGION_USER1, 0 ) /* v33 main cpu */
	ROM_LOAD("prg.223", 0x000000, 0x400000, CRC(b3dbcf98) SHA1(30d6ec2090531c8c579dff74c4898889902d7d87) )

	ROM_REGION( 0x100000, REGION_CPU1, ROMREGION_ERASE00 ) /* dummy 1MB CPU1 region for memory system */

	ROM_REGION( 0x040000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "fix.613", 0x000000, 0x040000, CRC(3da27e39) SHA1(3d446990bf36dd0a3f8fadb68b15bed54904c8b5) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "bg.612", 0x000000, 0x400000, CRC(162c61e9) SHA1(bd0a6a29804b84196ba6bf3402e9f30a25da9269) )

	ROM_REGION( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj1.724", 0x000000, 0x400000, CRC(7d218985) SHA1(777241a533defcbea3d7e735f309478d260bad52) )
	ROM_LOAD32_WORD( "obj2.725", 0x000002, 0x400000, CRC(891b24d6) SHA1(74f89b47b1ba6b84ddd96d1fae92fddad0ace342) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "pcm.099", 0x00000, 0x100000, CRC(97ca2907) SHA1(bfe8189300cf72089d0beaeab8b1a0a1a4f0a5b6) )

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "copx_d3.357", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) )

	ROM_REGION( 0x80, REGION_USER3, 0 ) /* EEPROM data */
	ROM_LOAD( "raidendx_eeprom-r2dx_v33.bin", 0x0000, 0x0080, CRC(0b34c0ca) SHA1(20612d5a1d819d3997ea47e8de7a194ec61b537d) )
ROM_END

ROM_START( r2dx_v33_r2 )
	ROM_REGION( 0x400000, REGION_USER1, 0 ) /* v33 main cpu */
	ROM_LOAD("prg.223", 0x000000, 0x400000, CRC(b3dbcf98) SHA1(30d6ec2090531c8c579dff74c4898889902d7d87) )

	ROM_REGION( 0x100000, REGION_CPU1, ROMREGION_ERASE00 ) /* dummy 1MB CPU1 region for memory system */

	ROM_REGION( 0x040000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD( "fix.613", 0x000000, 0x040000, CRC(3da27e39) SHA1(3d446990bf36dd0a3f8fadb68b15bed54904c8b5) )

	ROM_REGION( 0x400000, REGION_GFX2, 0 ) /* background gfx */
	ROM_LOAD( "bg.612", 0x000000, 0x400000, CRC(162c61e9) SHA1(bd0a6a29804b84196ba6bf3402e9f30a25da9269) )

	ROM_REGION( 0x800000, REGION_GFX3, 0 ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj1.724", 0x000000, 0x400000, CRC(7d218985) SHA1(777241a533defcbea3d7e735f309478d260bad52) )
	ROM_LOAD32_WORD( "obj2.725", 0x000002, 0x400000, CRC(891b24d6) SHA1(74f89b47b1ba6b84ddd96d1fae92fddad0ace342) )

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "pcm.099", 0x00000, 0x100000, CRC(97ca2907) SHA1(bfe8189300cf72089d0beaeab8b1a0a1a4f0a5b6) )

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "copx_d3.357", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) )

	ROM_REGION( 0x80, REGION_USER3, 0 ) /* EEPROM data */
	ROM_LOAD( "raidenii_eeprom-r2dx_v33.bin", 0x0000, 0x0080, CRC(ba454777) SHA1(101c5364e8664d17bfb1e759515d135a2673d67e) )
ROM_END

ROM_START( nzeroteam ) /* V33 SYSTEM TYPE_B hardware, uses SEI333 (AKA COPX-D3) for protection  */
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("seibu_1.u0224", 0x000000, 0x80000, CRC(ce1bcaf4) SHA1(1c340575e440b716caca8605cc5e1221060e3714) )
	ROM_LOAD16_BYTE("seibu_2.u0226", 0x000001, 0x80000, CRC(03f6e32d) SHA1(5363f20d515ff84346aa15f7b9d95c5805d81285) )

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "copx-d3.bin", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) ) /* Not from this set, but same data as Zero Team 2000 & Raiden II New */

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_3.u01019", 0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* Same as some of other Zero Team sets */
	ROM_CONTINUE(               0x010000, 0x08000 )    /* banked stuff */
	ROM_COPY( REGION_CPU2,       0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "seibu_5.u0616", 0x000000, 0x010000, CRC(ce68ba3c) SHA1(52830533711ec906bf4fe9d06e065ec80b25b4da) )
	ROM_LOAD16_BYTE( "seibu_6.u0617", 0x000001, 0x010000, CRC(cf44aea7) SHA1(e8d622fd5c10133fa563402daf0690fdff297f94) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_ERASEFF ) /* background gfx */
	ROM_LOAD( "back-1", 0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) ) /* Same as "MUSHA BACK-1" of other Zero Team sets */
	ROM_LOAD( "back-2", 0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) ) /* Same as "MUSHA BACK-2" of other Zero Team sets */

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj-1", 0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) ) /* Same as "MUSHA OBJ-1" of other Zero Team sets */
	ROM_LOAD32_WORD( "obj-2", 0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) ) /* Same as "MUSHA OBJ-2" of other Zero Team sets */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "seibu_4.u099", 0x00000, 0x40000, CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* Same as other Zero Team sets */

	ROM_REGION( 0x200, REGION_USER8, ROMREGION_DISPOSE ) /* PLDs */
	ROM_LOAD( "sysv33b-1.u0222.bin", 0x000, 0x117, CRC(f514a11f) SHA1(dd83ee1f511915d3d5f65375f34583be7fa1158b) )
	ROM_LOAD( "sysv33b-2.u0227.bin", 0x000, 0x117, CRC(d9f4612f) SHA1(0c507b28dc0f50a67cc12d63092067dc3f7f4679) )
ROM_END

ROM_START( nzeroteama ) /* V33 SYSTEM TYPE_B hardware, uses SEI333 (AKA COPX-D3) for protection  */
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("seibu_1.u0224", 0x000000, 0x80000, CRC(cb277b46) SHA1(86e99f14f7349bfabb13d1f540d6fa46748379dc) ) /* sldh */
	ROM_LOAD16_BYTE("seibu_2.u0226", 0x000001, 0x80000, CRC(6debbf78) SHA1(92d6f3bb59f72b40d3bc6d731866105a135574fb) ) /* sldh */

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "copx-d3.bin", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) ) /* Not from this set, but same data as Zero Team 2000 & Raiden II New */

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "seibu_3.u01019", 0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* Same as some of other Zero Team sets */
	ROM_CONTINUE(               0x010000, 0x08000 )    /* banked stuff */
	ROM_COPY( REGION_CPU2,       0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "seibu_5.u0616", 0x000000, 0x010000, CRC(ce68ba3c) SHA1(52830533711ec906bf4fe9d06e065ec80b25b4da) )
	ROM_LOAD16_BYTE( "seibu_6.u0617", 0x000001, 0x010000, CRC(cf44aea7) SHA1(e8d622fd5c10133fa563402daf0690fdff297f94) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_ERASEFF ) /* background gfx */
	ROM_LOAD( "back-1", 0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) ) /* Same as "MUSHA BACK-1" of other Zero Team sets */
	ROM_LOAD( "back-2", 0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) ) /* Same as "MUSHA BACK-2" of other Zero Team sets */

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj-1", 0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) ) /* Same as "MUSHA OBJ-1" of other Zero Team sets */
	ROM_LOAD32_WORD( "obj-2", 0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) ) /* Same as "MUSHA OBJ-2" of other Zero Team sets */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "seibu_4.u099", 0x00000, 0x40000, CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* Same as other Zero Team sets */

	ROM_REGION( 0x200, REGION_USER8, ROMREGION_DISPOSE ) /* PLDs */
	ROM_LOAD( "sysv33b-1.u0222.bin", 0x000, 0x117, CRC(f514a11f) SHA1(dd83ee1f511915d3d5f65375f34583be7fa1158b) )
	ROM_LOAD( "sysv33b-2.u0227.bin", 0x000, 0x117, CRC(d9f4612f) SHA1(0c507b28dc0f50a67cc12d63092067dc3f7f4679) )
ROM_END

ROM_START( nzeroteamb ) /* V33 SYSTEM TYPE_B hardware, uses SEI333 (AKA COPX-D3) for protection  */
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD16_BYTE("prg1", 0x000000, 0x80000, CRC(3c7d9410) SHA1(25f2121b6c2be73f11263934266901ed5d64d2ee) )
	ROM_LOAD16_BYTE("prg2", 0x000001, 0x80000, CRC(6cba032d) SHA1(bf5d488cd578fff09e62e3650efdee7658033e3f) )

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "copx-d3.bin", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) ) /* Not from this set, but same data as Zero Team 2000 & Raiden II New */

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "sound",    0x000000, 0x08000, CRC(7ec1fbc3) SHA1(48299d6530f641b18764cc49e283c347d0918a47) ) /* Same as some of other Zero Team sets */
	ROM_CONTINUE(         0x010000, 0x08000 )    /* banked stuff */
	ROM_COPY( REGION_CPU2, 0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "fix1", 0x000000, 0x010000, CRC(0c4895b0) SHA1(f595dbe5a19edb8a06ea60105ee26b95db4a2619) )
	ROM_LOAD16_BYTE( "fix2", 0x000001, 0x010000, CRC(07d8e387) SHA1(52f54a6a4830592784cdf643a5f255aa3db53e50) )

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_ERASEFF ) /* background gfx */
	ROM_LOAD( "back-1", 0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) ) /* Same as "MUSHA BACK-1" of other Zero Team sets */
	ROM_LOAD( "back-2", 0x100000, 0x080000, CRC(ce61c952) SHA1(52a843c8ba428b121fab933dd3b313b2894d80ac) ) /* Same as "MUSHA BACK-2" of other Zero Team sets */

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (encrypted) */
	ROM_LOAD32_WORD( "obj-1", 0x000000, 0x200000, CRC(45be8029) SHA1(adc164f9dede9a86b96a4d709e9cba7d2ad0e564) ) /* Same as "MUSHA OBJ-1" of other Zero Team sets */
	ROM_LOAD32_WORD( "obj-2", 0x000002, 0x200000, CRC(cb61c19d) SHA1(151a2ce9c32f3321a974819e9b165dddc31c8153) ) /* Same as "MUSHA OBJ-2" of other Zero Team sets */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "6.pcm", 0x00000, 0x40000, CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* Same as other Zero Team sets */

	ROM_REGION( 0x200, REGION_USER8, ROMREGION_DISPOSE ) /* PLDs */
	ROM_LOAD( "sysv33b-1.u0222.bin", 0x000, 0x117, CRC(f514a11f) SHA1(dd83ee1f511915d3d5f65375f34583be7fa1158b) )
	ROM_LOAD( "sysv33b-2.u0227.bin", 0x000, 0x117, CRC(d9f4612f) SHA1(0c507b28dc0f50a67cc12d63092067dc3f7f4679) )
ROM_END

ROM_START( zerotm2k ) /* V33 SYSTEM TYPE_C VER2 hardware, uses SEI333 (AKA COPX-D3) for protection  */
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* v30 main cpu */
	ROM_LOAD( "mt28f800b1.u0230", 0x000000, 0x100000, CRC(6ab49d8c) SHA1(d94ec9a46ff98a76c3372369246733268474de99) ) /* SMT rom, PCB silkscreened PRG01 */
	/* PCB has unpopulated socket space for two 27C040 at u0224 silkscreened PRG0 & u0226 silkscreened PRG1) */

	ROM_REGION( 0x20000, REGION_USER2, 0 ) /* SEI333 (AKA COPX-D3) data */
	ROM_LOAD( "mx27c1000mc.u0366", 0x00000, 0x20000, CRC(fa2cf3ad) SHA1(13eee40704d3333874b6e3da9ee7d969c6dc662a) ) /* PCB silkscreened 333ROM */

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* 64k code for sound Z80 */
	ROM_LOAD( "syz-02.u019", 0x000000, 0x08000, CRC(55371073) SHA1(f6e182fa64630595dc8c25ac820e12983cfbed12) ) /* PCB silkscreened SOUND */
	ROM_CONTINUE(            0x010000, 0x08000 )   /* banked stuff */
	ROM_COPY( REGION_CPU2,    0x000000, 0x018000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1, 0 ) /* chars */
	ROM_LOAD16_BYTE( "syz-04.u0616", 0x000000, 0x010000, CRC(3515a45f) SHA1(a25a7e23a5d9cf5a95a0d0e828848a8d223bdf51) ) /* PCB silkscreened FIX E */
	ROM_LOAD16_BYTE( "syz-03.u0617", 0x000001, 0x010000, CRC(02fbf9d7) SHA1(6eb4db1f89c9b003e7eed7bf39e6065b1c99447f) ) /* PCB silkscreened FIX O */

	ROM_REGION( 0x400000, REGION_GFX2, ROMREGION_ERASEFF ) /* background gfx */
	ROM_LOAD( "szy-05.u0614",     0x000000, 0x100000, CRC(8b7f9219) SHA1(3412b6f8a4fe245e521ddcf185a53f2f4520eb57) ) /* PCB silkscreened BG12, Same as "MUSHA BACK-1" */
	ROM_LOAD( "mt28f400b1.u0619", 0x100000, 0x080000, CRC(266acee6) SHA1(2a9da66c313a7536c7fb393134b9df0bb122cb2b) ) /* SMT rom, PCB silkscreened BG3 */
	/* PCB has an unpopulated socket rom space for a LH535A00D at u0615 for alt BG3 location */

	ROM_REGION32_LE( 0x800000, REGION_GFX3, ROMREGION_ERASEFF ) /* sprite gfx (NOT encrypted) */
	ROM_LOAD32_WORD( "musha_obj-1a.u0729", 0x000000, 0x200000, CRC(9b2cf68c) SHA1(cd8cb277091bfa125fd0f68410de39f72f1c7047) ) /* PCB silkscreened OBJ1 */
	ROM_LOAD32_WORD( "musha_obj-2a.u0730", 0x000002, 0x200000, CRC(fcabee05) SHA1(b2220c0311b3bd2fd44fb56fff7c27bed0816fe9) ) /* PCB silkscreened OBJ2 */
	/* PCB has unpopulated rom space for two SMT roms at u0734 & u0736 for alt OBJ1 & OBJ2 locations) */

	ROM_REGION( 0x100000, REGION_SOUND1, 0 ) /* ADPCM samples */
	ROM_LOAD( "szy-01.u099", 0x00000, 0x40000,  CRC(48be32b1) SHA1(969d2191a3c46871ee8bf93088b3cecce3eccf0c) ) /* PCB silkscreened PCM, Same as other Zero Team sets */
ROM_END

/* -----------------------------------------------------------------------
 * Driver inits
 * ----------------------------------------------------------------------- */

static void r2_set_bank_rom(void)
{
	UINT8 *cpu1 = memory_region(REGION_CPU1);
	r2_bank_rom = NULL;
	if (cpu1 && memory_region_length(REGION_CPU1) >= 0x200000)
		r2_bank_rom = cpu1 + 0x100000;
}

static DRIVER_INIT( raiden2 )
{
	r2_init_blending(raiden_blended_colors);
	r2_set_bank_rom();
	r2play_prepare_and_decrypt_sprites(memory_region(REGION_GFX3));
}

static DRIVER_INIT( raidendx )
{
	r2_init_blending(raiden_blended_colors);
	r2_bank_rom = NULL;
	r2play_prepare_and_decrypt_sprites(memory_region(REGION_GFX3));
}

static DRIVER_INIT( zeroteam )
{
	r2_init_blending(zeroteam_blended_colors);
	r2_set_bank_rom();
	zeroteam_prepare_and_decrypt_sprites(memory_region(REGION_GFX3));
}

static DRIVER_INIT( nzeroteam )
{
	r2_init_blending(zeroteam_blended_colors);
	zeroteam_prepare_and_decrypt_sprites(memory_region(REGION_GFX3));
}

static DRIVER_INIT( r2dx_v33 )
{
	r2_init_blending(raiden_blended_colors);
	r2play_prepare_and_decrypt_sprites(memory_region(REGION_GFX3));
	m_r2dxbank = 0;
	m_r2dxgameselect = 0;
}

/* no sprite encryption; the BG tile ROM has address lines 5 and 6 swapped */
static DRIVER_INIT( zerotm2k )
{
	UINT8 *src = memory_region(REGION_GFX2) + 0x100000;
	UINT8 *buffer = malloc(0x80000);
	int i;

	r2_init_blending(zeroteam_blended_colors);
	if (!buffer) return;
	for (i = 0; i < 0x80000; i++)
		buffer[i] = src[(i & ~0x60) | ((i & 0x20) << 1) | ((i & 0x40) >> 1)];
	memcpy(src, buffer, 0x80000);
	free(buffer);
}

/* -----------------------------------------------------------------------
 * Game drivers
 * ----------------------------------------------------------------------- */

GAME(  1993, raiden2,    0,         raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (US, set 1)" )
GAME(  1993, raiden2g,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden II (Germany)" )
GAME(  1993, raiden2gb,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Great Britain)" )
GAME(  1993, raiden2hk,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Metrotainment license)", "Raiden II (Hong Kong)" )
GAME(  1993, raiden2j,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Japan)" )
GAME(  1993, raiden2sw,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Switzerland)" )
GAME(  1993, raiden2u,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (US, set 2)" )
GAME(  1993, raiden2f,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (France)" )
GAME(  1993, raiden2nl,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Holland)" )
GAME(  1993, raiden2es,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Spain)" )
GAME(  1993, raiden2i,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Italy)" )
GAME(  1993, raiden2au,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (Australia)" )
GAME(  1993, raiden2e,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (easier, Korea)" )
GAME(  1993, raiden2eub, raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (easier, US set 3)" )
GAME(  1993, raiden2eua, raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (easier, US set 1)" )
GAME(  1993, raiden2eg,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden II (easier, Germany)" )
GAME(  1993, raiden2eup, raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (easier, US, prototype? 11-16)" )
GAME(  1993, raiden2ea,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (easier, Japan)" )
GAME(  1993, raiden2eu,  raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden II (easier, US set 2)" )
GAME(  1993, raiden2k,   raiden2,   raiden2,  raiden2,  raiden2,  ROT270, "Seibu Kaihatsu", "Raiden II (harder, Korea)" )
GAME(  1993, raiden2dx,  raiden2,   raidendx, raiden2,  raidendx, ROT270, "Seibu Kaihatsu", "Raiden II (harder, Raiden DX hardware, Korea)" )
GAME(  1994, raidendx,   0,         raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (UK)" )
GAME(  1994, raidendxg,  raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu (Tuning license)", "Raiden DX (Germany)" )
GAME(  1994, raidendxpt, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (Portugal)" )
GAME(  1994, raidendxa1, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu (Metrotainment license)", "Raiden DX (Hong Kong, set 1)" )
GAME(  1994, raidendxa2, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu (Metrotainment license)", "Raiden DX (Hong Kong, set 2)" )
GAME(  1994, raidendxk,  raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (Korea)" )
GAME(  1994, raidendxu,  raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu (Fabtek license)", "Raiden DX (US)" )
GAME(  1994, raidendxnl, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (Holland)" )
GAME(  1994, raidendxj,  raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (Japan, set 1)" )
GAME(  1994, raidendxja, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu", "Raiden DX (Japan, set 2)" )
GAME(  1994, raidendxch, raidendx,  raidendx, raidendx, raidendx, ROT270, "Seibu Kaihatsu (Ideal International Development Corp license)", "Raiden DX (China)" )
GAMEX( 1993, zeroteam,   0,         zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu (Fabtek license)", "Zero Team USA (US)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteama,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu", "Zero Team (Japan?, earlier?, set 1)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteamb,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu", "Zero Team (Japan?, later batteryless)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteamc,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu (Liang Hwa license)", "Zero Team (Taiwan)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteamd,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu (Dream Soft license)", "Zero Team (Korea)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteame,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu", "Zero Team (Japan?, earlier?, set 2)", GAME_NOT_WORKING )
GAMEX( 1993, zeroteams,  zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu", "Zero Team Selection", GAME_NOT_WORKING )
GAMEX( 1993, zeroteamsr, zeroteam,  zeroteam, zeroteam, zeroteam, ROT0, "Seibu Kaihatsu", "Zero Team Suicide Revival Kit", GAME_NOT_WORKING )
GAME(  1996, r2dx_v33,   0,         r2dx_v33, r2dx_v33, r2dx_v33, ROT270, "Seibu Kaihatsu", "Raiden II New / Raiden DX (newer V33 PCB) (Raiden DX EEPROM)" )
GAME(  1996, r2dx_v33_r2, r2dx_v33,  r2dx_v33, r2dx_v33, r2dx_v33, ROT270, "Seibu Kaihatsu", "Raiden II New / Raiden DX (newer V33 PCB) (Raiden II EEPROM)" )
GAME(  1997, nzeroteam,  zeroteam,  nzeroteam, nzeroteam, nzeroteam, ROT0, "Seibu Kaihatsu", "New Zero Team (V33 SYSTEM TYPE_B hardware)" )
GAME(  1997, nzeroteama, zeroteam,  nzeroteam, nzeroteam, nzeroteam, ROT0, "Seibu Kaihatsu (Zhongguo Shantou Yihuang license)", "New Zero Team (V33 SYSTEM TYPE_B hardware, Zhongguo Shantou Yihuang license)" )
GAME(  1997, nzeroteamb, zeroteam,  nzeroteam, nzeroteam, nzeroteam, ROT0, "Seibu Kaihatsu (Haoyunlai Trading Company license)", "New Zero Team (V33 SYSTEM TYPE_B hardware, Haoyunlai Trading Company license)" )
GAME(  2000, zerotm2k,   zeroteam,  zerotm2k, zerotm2k, zerotm2k, ROT0, "Seibu Kaihatsu", "Zero Team 2000" )
