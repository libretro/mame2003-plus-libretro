/***************************************************************************

    Sega 315-5881 decryption / decompression (Model 2 protection)

    Translated to C from current MAME (sega/315-5881_crypt.cpp,
    license BSD-3-Clause, copyright-holders: David Haywood).
    Only the Model 2 (little endian) interface is used here.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "machine/sega315_5881.h"

#ifndef BIT
#define BIT(x,n) (((x)>>(n))&1)
#endif


#define BUFFER_SIZE		2
#define LINE_SIZE		512
#define FLAG_COMPRESSED	0x20000

struct s5881_sbox
{
	UINT8 table[64];
	int inputs[6];		/* positions of the inputs bits, -1 means no input except from key */
	int outputs[2];		/* positions of the output bits */
};

static UINT16 (*s5881_read)(UINT32 addr);
static UINT32 key;
static UINT8 buffer[BUFFER_SIZE];
static UINT8 line_buffer_a[LINE_SIZE], line_buffer_b[LINE_SIZE];
static UINT8 *line_buffer = line_buffer_a, *line_buffer_prev = line_buffer_b;
static int line_buffer_swapped;
static UINT32 prot_cur_address;
static UINT16 subkey, dec_hist;
static UINT32 dec_header;
static int enc_ready, first_read;
static int buffer_pos, line_buffer_pos, line_buffer_size, buffer_bit, buffer_bit2;
static UINT8 buffer2[2];
static UINT16 buffer2a;
static int block_size, block_pos, block_numlines, done_compression;

static const struct s5881_sbox fn1_sboxes[4][4] = {
	{   /* 1st round */
		{
			{
				0,3,2,2,1,3,1,2,3,2,1,2,1,2,3,1,3,2,2,0,2,1,3,0,0,3,2,3,2,1,2,0,
				2,3,1,1,2,2,1,1,1,0,2,3,3,0,2,1,1,1,1,1,3,0,3,2,1,0,1,2,0,3,1,3,
			},
			{3,4,5,7,-1,-1},
			{0,4}
		},

		{
			{
				2,2,2,0,3,3,0,1,2,2,3,2,3,0,2,2,1,1,0,3,3,2,0,2,0,1,0,1,2,3,1,1,
				0,1,3,3,1,3,3,1,2,3,2,0,0,0,2,2,0,3,1,3,0,3,2,2,0,3,0,3,1,1,0,2,
			},
			{0,1,2,5,6,7},
			{1,6}
		},

		{
			{
				0,1,3,0,3,1,1,1,1,2,3,1,3,0,2,3,3,2,0,2,1,1,2,1,1,3,1,0,0,2,0,1,
				1,3,1,0,0,3,2,3,2,0,3,3,0,0,0,0,1,2,3,3,2,0,3,2,1,0,0,0,2,2,3,3,
			},
			{0,2,5,6,7,-1},
			{2,3}
		},

		{
			{
				3,2,1,2,1,2,3,2,0,3,2,2,3,1,3,3,0,2,3,0,3,3,2,1,1,1,2,0,2,2,0,1,
				1,3,3,0,0,3,0,3,0,2,1,3,2,1,0,0,0,1,1,2,0,1,0,0,0,1,3,3,2,0,3,3,
			},
			{1,2,3,4,6,7},
			{5,7}
		},
	},
	{   /* 2nd round */
		{
			{
				3,3,1,2,0,0,2,2,2,1,2,1,3,1,1,3,3,0,0,3,0,3,3,2,1,1,3,2,3,2,1,3,
				2,3,0,1,3,2,0,1,2,1,3,1,2,2,3,3,3,1,2,2,0,3,1,2,2,1,3,0,3,0,1,3,
			},
			{0,1,3,4,5,7},
			{0,4}
		},

		{
			{
				2,0,1,0,0,3,2,0,3,3,1,2,1,3,0,2,0,2,0,0,0,2,3,1,3,1,1,2,3,0,3,0,
				3,0,2,0,0,2,2,1,0,2,3,3,1,3,1,0,1,3,3,0,0,1,3,1,0,2,0,3,2,1,0,1,
			},
			{0,1,3,4,6,-1},
			{1,5}
		},

		{
			{
				2,2,2,3,1,1,0,1,3,3,1,1,2,2,2,0,0,3,2,3,3,0,2,1,2,2,3,0,1,3,0,0,
				3,2,0,3,2,0,1,0,0,1,2,2,3,3,0,2,2,1,3,1,1,1,1,2,0,3,1,0,0,2,3,2,
			},
			{1,2,5,6,7,6},
			{2,7}
		},

		{
			{
				0,1,3,3,3,1,3,3,1,0,2,0,2,0,0,3,1,2,1,3,1,2,3,2,2,0,1,3,0,3,3,3,
				0,0,0,2,1,1,2,3,2,2,3,1,1,2,0,2,0,2,1,3,1,1,3,3,1,1,3,0,2,3,0,0,
			},
			{2,3,4,5,6,7},
			{3,6}
		},
	},
	{   /* 3rd round */
		{
			{
				0,0,1,0,1,0,0,3,2,0,0,3,0,1,0,2,0,3,0,0,2,0,3,2,2,1,3,2,2,1,1,2,
				0,0,0,3,0,1,1,0,0,2,1,0,3,1,2,2,2,0,3,1,3,0,1,2,2,1,1,1,0,2,3,1,
			},
			{1,2,3,4,5,7},
			{0,5}
		},

		{
			{
				1,2,1,0,3,1,1,2,0,0,2,3,2,3,1,3,2,0,3,2,2,3,1,1,1,1,0,3,2,0,0,1,
				1,0,0,1,3,1,2,3,0,0,2,3,3,0,1,0,0,2,3,0,1,2,0,1,3,3,3,1,2,0,2,1,
			},
			{0,2,4,5,6,7},
			{1,6}
		},

		{
			{
				0,3,0,2,1,2,0,0,1,1,0,0,3,1,1,0,0,3,0,0,2,3,3,2,3,1,2,0,0,2,3,0,
				/* unused? */
				255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
			},
			{0,2,4,6,7,-1},
			{2,3}
		},

		{
			{
				0,0,1,0,0,1,0,2,3,3,0,3,3,2,3,0,2,2,2,0,3,2,0,3,1,0,0,3,3,0,0,0,
				2,2,1,0,2,0,3,2,0,0,3,1,3,3,0,0,2,1,1,2,1,0,1,1,0,3,1,2,0,2,0,3,
			},
			{0,1,2,3,6,-1},
			{4,7}
		},
	},
	{   /* 4th round */
		{
			{
				0,3,3,3,3,3,2,0,0,1,2,0,2,2,2,2,1,1,0,2,2,1,3,2,3,2,0,1,2,3,2,1,
				3,2,2,3,1,0,1,0,0,2,0,1,2,1,2,3,1,2,1,1,2,2,1,0,1,3,2,3,2,0,3,1,
			},
			{0,1,3,4,5,6},
			{0,5}
		},

		{
			{
				0,3,0,0,2,0,3,1,1,1,2,2,2,1,3,1,2,2,1,3,2,2,3,3,0,3,1,0,3,2,0,1,
				3,0,2,0,1,0,2,1,3,3,1,2,2,0,2,3,3,2,3,0,1,1,3,3,0,2,1,3,0,2,2,3,
			},
			{0,1,2,3,5,7},
			{1,7}
		},

		{
			{
				0,1,2,3,3,3,3,1,2,0,2,3,2,1,0,1,2,2,1,2,0,3,2,0,1,1,0,1,3,1,3,1,
				3,1,0,0,1,0,0,0,0,1,2,2,1,1,3,3,1,2,3,3,3,2,3,0,2,2,1,3,3,0,2,0,
			},
			{2,3,4,5,6,7},
			{2,3}
		},

		{
			{
				0,2,1,1,3,2,0,3,1,0,1,0,3,2,1,1,2,2,0,3,1,0,1,2,2,2,3,3,0,0,0,0,
				1,2,1,0,2,1,2,2,2,3,2,3,0,1,3,0,0,1,3,0,0,1,1,0,1,0,0,0,0,2,0,1,
			},
			{0,1,2,4,6,7},
			{4,6}
		},
	},
};


static const struct s5881_sbox fn2_sboxes[4][4] = {
	{   /* 1st round */
		{
			{
				3,3,0,1,0,1,0,0,0,3,0,0,1,3,1,2,0,3,3,3,2,1,0,1,1,1,2,2,2,3,2,2,
				2,1,3,3,1,3,1,1,0,0,1,2,0,2,2,1,1,2,3,1,2,1,3,1,2,2,0,1,3,0,2,2,
			},
			{1,3,4,5,6,7},
			{0,7}
		},

		{
			{
				0,1,3,0,1,1,2,3,2,0,0,3,2,1,3,1,3,3,0,0,1,0,0,3,0,3,3,2,3,2,0,1,
				3,2,3,2,2,1,3,1,1,1,0,3,3,2,2,1,1,2,0,2,0,1,1,0,1,0,1,1,2,0,3,0,
			},
			{0,3,5,6,5,0},
			{1,2}
		},

		{
			{
				0,2,2,1,0,1,2,1,2,0,1,2,3,3,0,1,3,1,1,2,1,2,1,3,3,2,3,3,2,1,0,1,
				0,1,0,2,0,1,1,3,2,0,3,2,1,1,1,3,2,3,0,2,3,0,2,2,1,3,0,1,1,2,2,2,
			},
			{0,2,3,4,7,-1},
			{3,4}
		},

		{
			{
				2,3,1,3,2,0,1,2,0,0,3,3,3,3,3,1,2,0,2,1,2,3,0,2,0,1,0,3,0,2,1,0,
				2,3,0,1,3,0,3,2,3,1,2,0,3,1,1,2,0,3,0,0,2,0,2,1,2,2,3,2,1,2,3,1,
			},
			{1,2,5,6,-1,-1},
			{5,6}
		},
	},
	{   /* 2nd round */
		{
			{
				2,3,1,3,1,0,3,3,3,2,3,3,2,0,0,3,2,3,0,3,1,1,2,3,1,1,2,2,0,1,0,0,
				2,1,0,1,2,0,1,2,0,3,1,1,2,3,1,2,0,2,0,1,3,0,1,0,2,2,3,0,3,2,3,0,
			},
			{0,1,4,5,6,7},
			{0,7}
		},

		{
			{
				0,2,2,0,2,2,0,3,2,3,2,1,3,2,3,3,1,1,0,0,3,0,2,1,1,3,3,2,3,2,0,1,
				1,2,3,0,1,0,3,0,3,1,0,2,1,2,0,3,2,3,1,2,2,0,3,2,3,0,0,1,2,3,3,3,
			},
			{0,2,3,6,7,-1},
			{1,5}
		},

		{
			{
				1,0,3,0,0,1,2,1,0,0,1,0,0,0,2,3,2,2,0,2,0,1,3,0,2,0,1,3,2,3,0,1,
				1,2,2,2,1,3,0,3,0,1,1,0,3,2,3,3,2,0,0,3,1,2,1,3,3,2,1,0,2,1,2,3,
			},
			{2,3,4,6,7,2},
			{2,3}
		},

		{
			{
				2,3,1,3,1,1,2,3,3,1,1,0,1,0,2,3,2,1,0,0,2,2,0,1,0,2,2,2,0,2,1,0,
				3,1,2,3,1,3,0,2,1,0,1,0,0,1,2,2,3,2,3,1,3,2,1,1,2,0,2,1,3,3,1,0,
			},
			{1,2,3,4,5,6},
			{4,6}
		},
	},
	{   /* 3rd round */
		{
			{
				0,3,0,1,3,0,0,2,1,0,1,3,2,2,2,0,3,3,3,0,2,2,0,3,0,0,2,3,0,3,2,1,
				3,3,0,3,0,2,3,3,1,1,1,0,2,2,1,1,3,0,3,1,2,0,2,0,0,0,3,2,1,1,0,0,
			},
			{1,4,5,6,7,5},
			{0,5}
		},

		{
			{
				0,3,0,1,3,0,3,1,3,2,2,2,3,0,3,2,2,1,2,2,0,3,2,2,0,0,2,1,1,3,2,3,
				2,3,3,1,2,0,1,2,2,1,0,0,0,0,2,3,1,2,0,3,1,3,1,2,3,2,1,0,3,0,0,2,
			},
			{0,2,3,4,6,7},
			{1,7}
		},

		{
			{
				2,2,0,3,0,3,1,0,1,1,2,3,2,3,1,0,0,0,3,2,2,0,2,3,1,3,2,0,3,3,1,3,
				/* unused? */
				255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
			},
			{1,2,4,7,2,-1},
			{2,4}
		},

		{
			{
				0,2,3,1,3,1,1,0,0,1,3,0,2,1,3,3,2,0,2,1,1,2,3,3,0,0,0,2,0,2,3,0,
				3,3,3,3,2,3,3,2,3,0,1,0,2,3,3,2,0,1,3,1,0,1,2,3,3,0,2,0,3,0,3,3,
			},
			{0,1,2,3,5,7},
			{3,6}
		},
	},
	{   /* 4th round */
		{
			{
				0,1,1,0,0,1,0,2,3,3,0,1,2,3,0,2,1,0,3,3,2,0,3,0,0,2,1,0,1,0,1,3,
				0,3,3,1,2,0,3,0,1,3,2,0,3,3,1,3,0,2,3,3,2,1,1,2,2,1,2,1,2,0,1,1,
			},
			{0,1,2,4,7,-1},
			{0,5}
		},

		{
			{
				2,0,0,2,3,0,2,3,3,1,1,1,2,1,1,0,0,2,1,0,0,3,1,0,0,3,3,0,1,0,1,2,
				0,2,0,2,0,1,2,3,2,1,1,0,3,3,3,3,3,3,1,0,3,0,0,2,0,3,2,0,2,2,0,1,
			},
			{0,1,3,5,6,-1},
			{1,3}
		},

		{
			{
				0,1,1,2,1,3,1,1,0,0,3,1,1,1,2,0,3,2,0,1,1,2,3,3,3,0,3,0,0,2,0,3,
				3,2,0,0,3,2,3,1,2,3,0,3,2,0,1,2,2,2,0,2,0,1,2,2,3,1,2,2,1,1,1,1,
			},
			{0,2,3,4,5,7},
			{2,7}
		},

		{
			{
				0,1,2,0,3,3,0,3,2,1,3,3,0,3,1,1,3,2,3,2,3,0,0,0,3,0,2,2,3,2,2,3,
				2,2,3,1,2,3,1,2,0,3,0,2,3,1,0,0,3,2,1,2,1,2,1,3,1,0,2,3,3,1,3,2,
			},
			{2,3,4,5,6,7},
			{4,6}
		},
	},
};

static const int fn1_game_key_scheduling[38][2] = {
	{1,29},  {1,71},  {2,4},   {2,54},  {3,8},   {4,56},  {4,73},  {5,11},
	{6,51},  {7,92},  {8,89},  {9,9},   {9,39},  {9,58},  {10,90}, {11,6},
	{12,64}, {13,49}, {14,44}, {15,40}, {16,69}, {17,15}, {18,23}, {18,43},
	{19,82}, {20,81}, {21,32}, {22,5},  {23,66}, {24,13}, {24,45}, {25,12},
	{25,35}, {26,61}, {27,10}, {27,59}, {28,25}, {29,86}
};

static const int fn2_game_key_scheduling[32][2] = {
	{0,0},   {1,3},   {2,11},  {3,20},  {4,22},  {5,23},  {6,29},  {7,38},
	{8,39},  {9,55},  {9,86},  {9,87},  {10,50}, {11,57}, {12,59}, {13,61},
	{14,63}, {15,67}, {16,72}, {17,83}, {18,88}, {19,94}, {20,35}, {21,17},
	{22,6},  {23,85}, {24,16}, {25,25}, {26,92}, {27,47}, {28,28}, {29,90}
};

static const int fn1_sequence_key_scheduling[20][2] = {
	{0,52},  {1,34},  {2,17},  {3,36}, {4,84},  {4,88},  {5,57},  {6,48},
	{6,68},  {7,76},  {8,83},  {9,30}, {10,22}, {10,41}, {11,38}, {12,55},
	{13,74}, {14,19}, {14,80}, {15,26}
};

static const int fn2_sequence_key_scheduling[16] = {77,34,8,42,36,27,69,66,13,9,79,31,49,7,24,64};

static const int fn2_middle_result_scheduling[16] = {1,10,44,68,74,78,81,95,2,4,30,40,41,51,53,58};


static const UINT8 trees[9][2][32] = {
	{
		{0x01,0x10,0x0f,0x05,0xc4,0x13,0x87,0x0a,0xcc,0x81,0xce,0x0c,0x86,0x0e,0x84,0xc2,
			0x11,0xc1,0xc3,0xcf,0x15,0xc8,0xcd,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc7,0x02,0x03,0x04,0x80,0x06,0x07,0x08,0x09,0xc9,0x0b,0x0d,0x82,0x83,0x85,0xc0,
			0x12,0xc6,0xc5,0x14,0x16,0xca,0xcb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x02,0x80,0x05,0x04,0x81,0x10,0x15,0x82,0x09,0x83,0x0b,0x0c,0x0d,0xdc,0x0f,0xde,
			0x1c,0xcf,0xc5,0xdd,0x86,0x16,0x87,0x18,0x19,0x1a,0xda,0xca,0xc9,0x1e,0xce,0xff,},
		{0x01,0x17,0x03,0x0a,0x08,0x06,0x07,0xc2,0xd9,0xc4,0xd8,0xc8,0x0e,0x84,0xcb,0x85,
			0x11,0x12,0x13,0x14,0xcd,0x1b,0xdb,0xc7,0xc0,0xc1,0x1d,0xdf,0xc3,0xc6,0xcc,0xff,},
	},
	{
		{0xc6,0x80,0x03,0x0b,0x05,0x07,0x82,0x08,0x15,0xdc,0xdd,0x0c,0xd9,0xc2,0x14,0x10,
			0x85,0x86,0x18,0x16,0xc5,0xc4,0xc8,0xc9,0xc0,0xcc,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0x01,0x02,0x12,0x04,0x81,0x06,0x83,0xc3,0x09,0x0a,0x84,0x11,0x0d,0x0e,0x0f,0x19,
			0xca,0xc1,0x13,0xd8,0xda,0xdb,0x17,0xde,0xcd,0xcb,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0x80,0x0d,0x04,0x05,0x15,0x83,0x08,0xd9,0x10,0x0b,0x0c,0x84,0x0e,0xc0,0x14,
			0x12,0xcb,0x13,0xca,0xc8,0xc2,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc5,0x02,0x03,0x07,0x81,0x06,0x82,0xcc,0x09,0x0a,0xc9,0x11,0xc4,0x0f,0x85,0xd8,
			0xda,0xdb,0xc3,0xdc,0xdd,0xc1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0x80,0x06,0x0c,0x05,0x81,0xd8,0x84,0x09,0xdc,0x0b,0x0f,0x0d,0x0e,0x10,0xdb,
			0x11,0xca,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc4,0x02,0x03,0x04,0xcb,0x0a,0x07,0x08,0xd9,0x82,0xc8,0x83,0xc0,0xc1,0xda,0xc2,
			0xc9,0xc3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0x02,0x06,0x0a,0x83,0x0b,0x07,0x08,0x09,0x82,0xd8,0x0c,0xd9,0xda,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc3,0x80,0x03,0x04,0x05,0x81,0xca,0xc8,0xdb,0xc9,0xc0,0xc1,0x0d,0xc2,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0x02,0x03,0x04,0x81,0x07,0x08,0xd8,0xda,0xd9,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc2,0x80,0x05,0xc9,0xc8,0x06,0x82,0xc0,0x09,0xc1,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0x80,0x04,0xc8,0xc0,0xd9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc1,0x02,0x03,0x81,0x05,0xd8,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
	{
		{0x01,0xd8,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
		{0xc0,0x80,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,},
	},
};

static int feistel_function(int input, const struct s5881_sbox *sboxes, UINT32 subkeys)
{
	int k,m;
	int aux;
	int result=0;

	for (m=0; m<4; ++m) { /* 4 sboxes */
		for (k=0, aux=0; k<6; ++k)
			if (sboxes[m].inputs[k]!=-1)
				aux |= BIT(input, sboxes[m].inputs[k]) << k;

		aux = sboxes[m].table[(aux^subkeys)&0x3f];

		for (k=0; k<2; ++k)
			result |= BIT(aux,k) << sboxes[m].outputs[k];

		subkeys >>=6;
	}

	return result;
}

/**************************
This implementation is an "educational" version. It must be noted that it can be speed-optimized in a number of ways.
The most evident one is to factor out the parts of the key-scheduling that must only be done once (like the game-key &
sequence-key parts) as noted in the comments inlined in the function. More sophisticated speed-ups can be gained by
noticing that the weak key-scheduling would allow to create some pregenerated look-up tables for doing most of the work
of the function. Even so, it would still be pretty slow, so caching techniques could be a wiser option here.
**************************/

static UINT16 block_decrypt(UINT32 game_key, UINT16 sequence_key, UINT16 counter, UINT16 data)
{
	int j;
	int aux, aux2;
	int A, B;
	int middle_result;
	UINT32 fn1_subkeys[4];
	UINT32 fn2_subkeys[4];

	/* Game-key scheduling; this could be done just once per game at initialization time */
	memset(fn1_subkeys, 0, sizeof(UINT32) * 4);
	memset(fn2_subkeys, 0, sizeof(UINT32) * 4);

	for (j = 0; j < 38; ++j) {
		if (BIT(game_key, fn1_game_key_scheduling[j][0]) != 0) {
			aux = fn1_game_key_scheduling[j][1] % 24;
			aux2 = fn1_game_key_scheduling[j][1] / 24;
			fn1_subkeys[aux2] ^= (1 << aux);
		}
	}

	for (j = 0; j < 32; ++j) {
		if (BIT(game_key, fn2_game_key_scheduling[j][0]) != 0) {
			aux = fn2_game_key_scheduling[j][1] % 24;
			aux2 = fn2_game_key_scheduling[j][1] / 24;
			fn2_subkeys[aux2] ^= (1 << aux);
		}
	}
	/********************************************************/

	/* Sequence-key scheduling; this could be done just once per decryption run */
	for (j = 0; j < 20; ++j) {
		if (BIT(sequence_key, fn1_sequence_key_scheduling[j][0]) != 0) {
			aux = fn1_sequence_key_scheduling[j][1] % 24;
			aux2 = fn1_sequence_key_scheduling[j][1] / 24;
			fn1_subkeys[aux2] ^= (1 << aux);
		}
	}

	for (j = 0; j < 16; ++j) {
		if (BIT(sequence_key, j) != 0) {
			aux = fn2_sequence_key_scheduling[j] % 24;
			aux2 = fn2_sequence_key_scheduling[j] / 24;
			fn2_subkeys[aux2] ^= (1 << aux);
		}
	}

	/**************************************************************/

	/* First Feistel Network */

	aux = BITSWAP16(counter, 5, 12, 14, 13, 9, 3, 6, 4, 8, 1, 15, 11, 0, 7, 10, 2);

	/* 1st round */
	B = aux >> 8;
	A = (aux & 0xff) ^ feistel_function(B, fn1_sboxes[0], fn1_subkeys[0]);

	/* 2nd round */
	B ^= feistel_function(A, fn1_sboxes[1], fn1_subkeys[1]);

	/* 3rd round */
	A ^= feistel_function(B, fn1_sboxes[2], fn1_subkeys[2]);

	/* 4th round */
	B ^= feistel_function(A, fn1_sboxes[3], fn1_subkeys[3]);

	middle_result = (B << 8) | A;


	/* Middle-result-key sheduling */
	for (j = 0; j < 16; ++j) {
		if (BIT(middle_result, j) != 0) {
			aux = fn2_middle_result_scheduling[j] % 24;
			aux2 = fn2_middle_result_scheduling[j] / 24;
			fn2_subkeys[aux2] ^= (1 << aux);
		}
	}
	/*********************/

	/* Second Feistel Network */

	aux = BITSWAP16(data, 14, 3, 8, 12, 13, 7, 15, 4, 6, 2, 9, 5, 11, 0, 1, 10);

	/* 1st round */
	B = aux >> 8;
	A = (aux & 0xff) ^ feistel_function(B, fn2_sboxes[0], fn2_subkeys[0]);

	/* 2nd round */
	B ^= feistel_function(A, fn2_sboxes[1], fn2_subkeys[1]);

	/* 3rd round */
	A ^= feistel_function(B, fn2_sboxes[2], fn2_subkeys[2]);

	/* 4th round */
	B ^= feistel_function(A, fn2_sboxes[3], fn2_subkeys[3]);

	aux = (B << 8) | A;

	aux = BITSWAP16(aux, 15, 7, 6, 14, 13, 12, 5, 4, 3, 2, 11, 10, 9, 1, 0, 8);

	return aux;
}


static UINT16 get_decrypted_16(void)
{
	UINT16 enc, dec, res;

	enc = s5881_read(prot_cur_address);

	dec = block_decrypt(key, subkey, prot_cur_address, enc);
	res = (dec & 3) | (dec_hist & 0xfffc);
	dec_hist = dec;

	prot_cur_address ++;

/*  printf("get_decrypted_16 %04x\n", res); */

	return res;
}


static void enc_start(void)
{
	int blocky;
	block_pos = 0;
	done_compression = 0;
	buffer_pos = BUFFER_SIZE;

	if (buffer_bit2 < 14) /* if we have remaining bits in the decompression buffer we shouldn't read the next word yet but should instead use the bits we have?? (twcup98 needs this, twsoc98 value is 14 on title screen and expects normal logic) */
	{
/*      printf("buffer_bit2 is %d\n", buffer_bit2); */
		dec_header = (buffer2a & 0x0003) << 16;
	}
	else
	{
		dec_hist = 0; /* seems to be needed by astrass at least otherwise any call after the first one will be influenced by the one before it. */
		dec_header = get_decrypted_16() << 16;
	}

	dec_header |= get_decrypted_16();

	/* the lower header bits are 2 values that multiply together to get the current stream length */
	/* in astrass the first block is 0xffff (for a 0x10000 block) followed by 0x3f3f (for a 0x1000 block) */
	/* etc. after each block a new header must be read, it looks like compressed and uncompressed blocks */
	/* can be mixed like this, I don't know if the length is src length of decompressed length. */
	/* deathcox and others confirm format as 0x20000 bit as compressed bit, 0x1ff00 bits as block size 1, 0x000ff bits as block size 2 */
	/* for compressed streams the 'line size' is block size 1. */

	block_numlines = ((dec_header & 0x000000ff) >> 0) + 1;
	blocky = ((dec_header & 0x0001ff00) >> 8) + 1;
	block_size = block_numlines * blocky;

	if(dec_header & FLAG_COMPRESSED) {
		line_buffer_size = blocky;
		line_buffer_pos = line_buffer_size;
		buffer_bit = 7;
		buffer_bit2 = 15;
	}

	enc_ready = 1;
}

static void enc_fill(void)
{
	int i;
	for(i = 0; i != BUFFER_SIZE; i+=2) {
		UINT16 val = get_decrypted_16();
		buffer[i] = val;
		buffer[i+1] = val >> 8;
		block_pos+=2;

		if (!(dec_header & FLAG_COMPRESSED))
		{
			if (block_pos == block_size)
			{
				/* if we reach the size specified we need to read a new header */
				/* todo: for compressed blocks this depends on OUTPUT size, not input size, so things get messy */

				enc_start();
			}
		}
	}
	buffer_pos = 0;
}

/* node format
0xxxxxxx - next node index
1a0bbccc - end node
           a - 0 = repeat
               1 = fetch
           b - if a = 1
               00 - fetch  0
               01 - fetch  1
               11 - fetch -1
               if a = 0
               000
           c - repeat/fetch counter
               count = ccc + 1
11111111 - empty node
*/


static int get_compressed_bit(void)
{
	int res;
/*  if(buffer_pos == BUFFER_SIZE) */
/*      enc_fill(); */

	if (buffer_bit2 == 15)
	{
		buffer_bit2 = 0;
		buffer2a = get_decrypted_16();
		buffer2[0] = buffer2a;
		buffer2[1] = buffer2a >> 8;
	/*  block_pos+=2; */
		buffer_pos = 0;

	}
	else
	{
		buffer_bit2++;
	}

/*  if (buffer_bit ==7) printf("using byte %02x\n", buffer2[(buffer_pos&1) ^ 1]); */

	res = (buffer2[(buffer_pos&1)^1] >> buffer_bit) & 1;
	buffer_bit--;
	if(buffer_bit == -1) {
		buffer_bit = 7;
		buffer_pos++;
	}
	return res;
}
static void line_fill(void)
{
	UINT8 *lp = line_buffer;
	UINT8 *lc = line_buffer_prev;
	int i, j;

	{ UINT8 *t = line_buffer; line_buffer = line_buffer_prev; line_buffer_prev = t; }

	line_buffer_pos = 0;

	for(i=0; i != line_buffer_size;) {
		/* vlc 0: start of line */
		/* vlc 1: interior of line */
		/* vlc 2-9: 7-1 bytes from end of line */

		int slot = i ? i < line_buffer_size - 7 ? 1 : (i & 7) + 1 : 0;

		UINT32 tmp = 0;
		while (!(tmp&0x80))
			if(get_compressed_bit())
				tmp = trees[slot][1][tmp];
			else
				tmp = trees[slot][0][tmp];
		if(tmp != 0xff) {
			int count = (tmp & 7) + 1;

			if(tmp&0x40) {
				/* Copy from previous line */

				static int offsets[4] = {0, 1, 0, -1};
				int offset = offsets[(tmp & 0x18) >> 3];
				for(j=0; j != count; j++) {
					lc[i^1] = lp[((i+offset) % line_buffer_size)^1];
					i++;
				}

			} else {
				/* Get a byte in the stream and write n times */
				UINT8 byte;
				byte =         get_compressed_bit()  << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte = (byte | get_compressed_bit()) << 1;
				byte =  byte | get_compressed_bit();
				for(j=0; j != count; j++)
					lc[(i++)^1] = byte;

			}
		}
	}

	block_pos++;
	if (block_numlines == block_pos)
	{
		done_compression = 1;
	}
}


/*************************************
 *  Interface
 *************************************/

static void s5881_postload( void )
{
	line_buffer = line_buffer_swapped ? line_buffer_b : line_buffer_a;
	line_buffer_prev = line_buffer_swapped ? line_buffer_a : line_buffer_b;
}

static void s5881_presave( void )
{
	line_buffer_swapped = ( line_buffer == line_buffer_b );
}

void sega315_5881_init( UINT32 game_key, UINT16 (*read)( UINT32 addr ) )
{
	key = game_key;
	s5881_read = read;

	state_save_register_UINT8( "315_5881", 0, "buffer", buffer, BUFFER_SIZE );
	state_save_register_UINT8( "315_5881", 0, "line_a", line_buffer_a, LINE_SIZE );
	state_save_register_UINT8( "315_5881", 0, "line_b", line_buffer_b, LINE_SIZE );
	state_save_register_int( "315_5881", 0, "swapped", &line_buffer_swapped );
	state_save_register_UINT32( "315_5881", 0, "address", &prot_cur_address, 1 );
	state_save_register_UINT16( "315_5881", 0, "subkey", &subkey, 1 );
	state_save_register_UINT16( "315_5881", 0, "dec_hist", &dec_hist, 1 );
	state_save_register_UINT32( "315_5881", 0, "dec_header", &dec_header, 1 );
	state_save_register_int( "315_5881", 0, "enc_ready", &enc_ready );
	state_save_register_int( "315_5881", 0, "first_read", &first_read );
	state_save_register_int( "315_5881", 0, "buffer_pos", &buffer_pos );
	state_save_register_int( "315_5881", 0, "line_buffer_pos", &line_buffer_pos );
	state_save_register_int( "315_5881", 0, "line_buffer_size", &line_buffer_size );
	state_save_register_int( "315_5881", 0, "buffer_bit", &buffer_bit );
	state_save_register_int( "315_5881", 0, "buffer_bit2", &buffer_bit2 );
	state_save_register_UINT8( "315_5881", 0, "buffer2", buffer2, 2 );
	state_save_register_UINT16( "315_5881", 0, "buffer2a", &buffer2a, 1 );
	state_save_register_int( "315_5881", 0, "block_size", &block_size );
	state_save_register_int( "315_5881", 0, "block_pos", &block_pos );
	state_save_register_int( "315_5881", 0, "block_numlines", &block_numlines );
	state_save_register_int( "315_5881", 0, "done_compression", &done_compression );
	state_save_register_func_presave( s5881_presave );
	state_save_register_func_postload( s5881_postload );
}

void sega315_5881_reset( void )
{
	memset( buffer, 0, sizeof( buffer ) );
	memset( line_buffer_a, 0, sizeof( line_buffer_a ) );
	memset( line_buffer_b, 0, sizeof( line_buffer_b ) );
	line_buffer = line_buffer_a;
	line_buffer_prev = line_buffer_b;
	prot_cur_address = 0;
	subkey = 0;
	dec_hist = 0;
	dec_header = 0;
	enc_ready = 0;
	first_read = 0;
	buffer_pos = 0;
	line_buffer_pos = 0;
	line_buffer_size = 0;
	buffer_bit = 0;
}

UINT16 sega315_5881_ready_r( void )
{
	return 0;	/* bit 0: busy */
}

void sega315_5881_addrlo_w( UINT16 data )
{
	prot_cur_address = ( prot_cur_address & 0xffff0000 ) | data;
	enc_ready = 0;
	first_read = 1;
}

void sega315_5881_addrhi_w( UINT16 data )
{
	/* only 0 seen */
	prot_cur_address &= 0x0000ffff;
	enc_ready = 0;
	buffer_bit = 7;
	buffer_bit2 = 15;
	first_read = 1;
}

void sega315_5881_subkey_w( UINT16 data )
{
	subkey = data;
	enc_ready = 0;
}

static UINT16 do_decrypt( void )
{
	UINT8 *base;

	if( !enc_ready )
		enc_start();
	if( dec_header & FLAG_COMPRESSED )
	{
		if( line_buffer_pos == line_buffer_size )
		{
			if( done_compression == 1 )
				enc_start();
			line_fill();
		}
		base = line_buffer + line_buffer_pos;
		line_buffer_pos += 2;
	}
	else
	{
		if( buffer_pos == BUFFER_SIZE )
			enc_fill();
		base = buffer + buffer_pos;
		buffer_pos += 2;
	}
	return ( base[ 0 ] << 8 ) | base[ 1 ];
}

void sega315_5881_set_addr_low( UINT16 data )
{
	prot_cur_address = ( prot_cur_address & 0xffff0000 ) | data;
	enc_ready = 0;
}

void sega315_5881_set_addr_high( UINT16 data )
{
	prot_cur_address = ( prot_cur_address & 0x0000ffff ) | ( (UINT32)data << 16 );
	enc_ready = 0;
	buffer_bit = 7;
	buffer_bit2 = 15;
}

void sega315_5881_set_subkey( UINT16 data )
{
	subkey = data;
	enc_ready = 0;
}

UINT16 sega315_5881_do_decrypt( void )
{
	return do_decrypt();
}

/* little endian read: the decrypted word with its bytes swapped */
UINT16 sega315_5881_decrypt_r( void )
{
	UINT16 v;
	if( first_read )
	{
		/* a dummy value before the start of the stream */
		first_read = 0;
		return 0;
	}
	v = do_decrypt();
	return ( ( v & 0xff00 ) >> 8 ) | ( ( v & 0x00ff ) << 8 );
}
