/***************************************************************************

	PSX GPU

	Preliminary software renderer by smf.
	Thanks to Ryan Holtz, Pete B & Farfetch'd.

	Supports:
	  type 1 1024x1024 framebuffer (CXD8538Q)
	  type 2 1024x512 framebuffer
	  type 2 1024x1024 framebuffer (CXD8514Q/CXD8561Q/CXD8654Q)

	Debug Keys:
		M toggles mesh viewer.
		V toggles vram viewer.
		I toggles interleave in vram viewer.

***************************************************************************/

#include "driver.h"
#include "state.h"
#include "includes/psx.h"
#include "usrintrf.h"

/* drawing on a second thread: hosts with POSIX threads and GCC/clang atomics */
#if defined( __GNUC__ ) && ( defined( __linux__ ) || defined( __APPLE__ ) ) && !defined( __EMSCRIPTEN__ )
#define PSX_GPU_THREADS ( 1 )
#include <pthread.h>
#include <sched.h>
#else
#define PSX_GPU_THREADS ( 0 )
#endif

static int psx_gpu_thread_wanted;	/* set by the driver (core option) */
static void psx_gpu_sync( void );
static void psx_gq_start( void );
static void psx_gq_stop( void );

#define STOP_ON_ERROR ( 0 )

#define VERBOSE_LEVEL ( 0 )

static INLINE void verboselog( int n_level, const char *s_fmt, ... )
{
	if( VERBOSE_LEVEL >= n_level )
	{
		va_list v;
		char buf[ 32768 ];
		va_start( v, s_fmt );
		vsprintf( buf, s_fmt, v );
		va_end( v );
		log_cb(RETRO_LOG_DEBUG, LOGPRE  "%08x: %s", activecpu_get_pc(), buf );
	}
}

struct FLATVERTEX
{
	PAIR n_coord;
};

struct GOURAUDVERTEX
{
	PAIR n_bgr;
	PAIR n_coord;
};

struct FLATTEXTUREDVERTEX
{
	PAIR n_coord;
	PAIR n_texture;
};

struct GOURAUDTEXTUREDVERTEX
{
	PAIR n_bgr;
	PAIR n_coord;
	PAIR n_texture;
};

typedef union
{
	UINT32 n_entry[ 16 ];

	struct
	{
		PAIR n_cmd;
		struct FLATVERTEX vertex[ 2 ];
		PAIR n_size;
	} MoveImage;

	struct
	{
		PAIR n_bgr;
		PAIR n_coord;
		PAIR n_size;
	} FlatRectangle;

	struct
	{
		PAIR n_bgr;
		PAIR n_coord;
		PAIR n_texture;
	} Sprite8x8;

	struct
	{
		PAIR n_bgr;
		PAIR n_coord;
		PAIR n_texture;
	} Sprite16x16;

	struct
	{
		PAIR n_bgr;
		PAIR n_coord;
		PAIR n_texture;
		PAIR n_size;
	} FlatTexturedRectangle;

	struct
	{
		PAIR n_bgr;
		struct FLATVERTEX vertex[ 4 ];
	} FlatPolygon;

	struct
	{
		struct GOURAUDVERTEX vertex[ 4 ];
	} GouraudPolygon;

	struct
	{
		PAIR n_bgr;
		struct FLATVERTEX vertex[ 2 ];
	} MonochromeLine;

	struct
	{
		struct GOURAUDVERTEX vertex[ 2 ];
	} GouraudLine;

	struct
	{
		PAIR n_bgr;
		struct FLATTEXTUREDVERTEX vertex[ 4 ];
	} FlatTexturedPolygon;

	struct
	{
		struct GOURAUDTEXTUREDVERTEX vertex[ 4 ];
	} GouraudTexturedPolygon;

	struct
	{
		PAIR n_bgr;
		struct FLATVERTEX vertex;
	} Dot;
} psx_gpu_packet;

static psx_gpu_packet m_packet;

static UINT16 *m_p_vram;
static UINT32 m_n_vram_size;
static UINT32 m_n_vram_height;
static UINT32 m_n_vram_width;
static UINT32 m_n_gpu_buffer_offset;
static UINT32 m_n_vramx;
static UINT32 m_n_vramy;
static UINT32 m_n_twy;
static UINT32 m_n_twx;
static UINT32 m_n_twh;
static UINT32 m_n_tww;
static UINT32 m_n_drawarea_x1;
static UINT32 m_n_drawarea_y1;
static UINT32 m_n_drawarea_x2;
static UINT32 m_n_drawarea_y2;
static UINT32 m_n_horiz_disstart;
static UINT32 m_n_horiz_disend;
static UINT32 m_n_vert_disstart;
static UINT32 m_n_vert_disend;
static UINT32 m_b_reverseflag;
static INT32 m_n_drawoffset_x;
static INT32 m_n_drawoffset_y;
static UINT32 m_n_displaystartx;
static UINT32 m_n_displaystarty;
static UINT32 m_n_gpustatus;
static UINT32 m_n_gpuinfo;
static UINT32 m_n_screenwidth;
static UINT32 m_n_screenheight;
static UINT32 m_n_drawmode;

#define MAX_LEVEL ( 32 )
#define MID_LEVEL ( ( MAX_LEVEL / 2 ) << 8 )
#define MAX_SHADE ( 0x100 )
#define MID_SHADE ( 0x80 )

static int m_n_gputype;

static UINT16 *m_p_p_vram[ 1024 ];

static UINT16 m_p_n_redshade[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_greenshade[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_blueshade[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_redlevel[ 0x10000 ];
static UINT16 m_p_n_greenlevel[ 0x10000 ];
static UINT16 m_p_n_bluelevel[ 0x10000 ];

static UINT16 m_p_n_f025[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_f05[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_f1[ MAX_LEVEL * MAX_SHADE ];
static UINT16 m_p_n_redb05[ 0x10000 ];
static UINT16 m_p_n_greenb05[ 0x10000 ];
static UINT16 m_p_n_blueb05[ 0x10000 ];
static UINT16 m_p_n_redb1[ 0x10000 ];
static UINT16 m_p_n_greenb1[ 0x10000 ];
static UINT16 m_p_n_blueb1[ 0x10000 ];
static UINT16 m_p_n_redaddtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_greenaddtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_blueaddtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_redsubtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_greensubtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_bluesubtrans[ MAX_LEVEL * MAX_LEVEL ];
static UINT16 m_p_n_nextpointlist4[] = { 1, 3, 0, 2 };
static UINT16 m_p_n_prevpointlist4[] = { 2, 0, 3, 1 };
static UINT16 m_p_n_nextpointlist3[] = { 1, 2, 0 };
static UINT16 m_p_n_prevpointlist3[] = { 2, 0, 1 };

#define SINT11( x ) ( ( (INT32)( x ) << 21 ) >> 21 )

#define ADJUST_COORD( a ) \
	a.w.l = COORD_X( a ) + m_n_drawoffset_x; \
	a.w.h = COORD_Y( a ) + m_n_drawoffset_y;

#define COORD_X( a ) ( (INT16)a.w.l )
#define COORD_Y( a ) ( (INT16)a.w.h )
#define SIZE_W( a ) ( a.w.l )
#define SIZE_H( a ) ( a.w.h )
#define BGR_C( a ) ( a.b.h3 )
#define BGR_B( a ) ( a.b.h2 )
#define BGR_G( a ) ( a.b.h )
#define BGR_R( a ) ( a.b.l )
#define TEXTURE_V( a ) ( (UINT8)a.b.h )
#define TEXTURE_U( a ) ( (UINT8)a.b.l )

PALETTE_INIT( psx )
{
	UINT32 n_r;
	UINT32 n_g;
	UINT32 n_b;
	UINT32 n_colour;

	for( n_colour = 0; n_colour < 0x10000; n_colour++ )
	{
		n_r = ( ( n_colour & 0x1f ) * 0xff ) / 0x1f;
		n_g = ( ( ( n_colour >> 5 ) & 0x1f ) * 0xff ) / 0x1f;
		n_b = ( ( ( n_colour >> 10 ) & 0x1f ) * 0xff ) / 0x1f;

		palette_set_color( n_colour, n_r, n_g, n_b );
	}
}

#if defined( MAME_DEBUG )

#define DEBUG_COORDS ( 10 )
static struct mame_bitmap *debugmesh;
static int m_b_debugclear;
static int m_b_debugmesh;
static int m_n_debugskip;
static int m_b_debugtexture;
static int m_n_debuginterleave;
static int m_n_debugcoord;
static int m_n_debugcoordx[ DEBUG_COORDS ];
static int m_n_debugcoordy[ DEBUG_COORDS ];

#define DEBUG_MAX ( 512 )

static void DebugMeshInit( void )
{
	m_b_debugmesh = 0;
	m_b_debugtexture = 0;
	m_n_debuginterleave = -1;
	m_b_debugclear = 1;
	m_n_debugcoord = 0;
	m_n_debugskip = 0;
	debugmesh = auto_bitmap_alloc_depth( 1024, 1024, 16 );
}

static void DebugMesh( int n_coordx, int n_coordy )
{
	int n_coord;
	int n_colour;

	if( m_b_debugclear )
	{
		fillbitmap( debugmesh, 0x0000, NULL );
		m_b_debugclear = 0;
	}

	if( m_n_debugcoord < DEBUG_COORDS )
	{
		n_coordx += m_n_displaystartx;
		n_coordy += m_n_displaystarty;

		n_coordx *= 511;
		n_coordx /= DEBUG_MAX - 1;
		n_coordx += 256;
		n_coordy *= 511;
		n_coordy /= DEBUG_MAX - 1;
		n_coordy += 256;

		m_n_debugcoordx[ m_n_debugcoord ] = n_coordx;
		m_n_debugcoordy[ m_n_debugcoord ] = n_coordy;
		m_n_debugcoord++;
	}

	n_colour = 0x1f;
	for( n_coord = 0; n_coord < m_n_debugcoord; n_coord++ )
	{
		if( n_coordx != m_n_debugcoordx[ n_coord ] ||
			n_coordy != m_n_debugcoordy[ n_coord ] )
		{
			break;
		}
	}
	if( n_coord == m_n_debugcoord && m_n_debugcoord > 1 )
	{
		n_colour = 0xffff;
	}
	for( n_coord = 0; n_coord < m_n_debugcoord; n_coord++ )
	{
		PAIR n_x;
		PAIR n_y;
		INT32 n_xstart;
		INT32 n_ystart;
		INT32 n_xend;
		INT32 n_yend;
		INT32 n_xlen;
		INT32 n_ylen;
		INT32 n_len;
		INT32 n_dx;
		INT32 n_dy;

		n_xstart = m_n_debugcoordx[ n_coord ];
		n_xend = n_coordx;
		if( n_xend > n_xstart )
		{
			n_xlen = n_xend - n_xstart;
		}
		else
		{
			n_xlen = n_xstart - n_xend;
		}

		n_ystart = m_n_debugcoordy[ n_coord ];
		n_yend = n_coordy;
		if( n_yend > n_ystart )
		{
			n_ylen = n_yend - n_ystart;
		}
		else
		{
			n_ylen = n_ystart - n_yend;
		}

		if( n_xlen > n_ylen )
		{
			n_len = n_xlen;
		}
		else
		{
			n_len = n_ylen;
		}

		n_x.w.h = n_xstart; n_x.w.l = 0;
		n_y.w.h = n_ystart; n_y.w.l = 0;

		if( n_len == 0 )
		{
			n_len = 1;
		}

		n_dx = (INT32)( ( n_xend << 16 ) - n_x.d ) / n_len;
		n_dy = (INT32)( ( n_yend << 16 ) - n_y.d ) / n_len;
		while( n_len > 0 )
		{
			if( (INT16)n_x.w.h >= 0 &&
				(INT16)n_y.w.h >= 0 &&
				(INT16)n_x.w.h <= 1023 &&
				(INT16)n_y.w.h <= 1023 )
			{
				if( read_pixel( debugmesh, n_x.w.h, n_y.w.h ) != 0xffff )
				{
					plot_pixel( debugmesh, n_x.w.h, n_y.w.h, n_colour );
				}
			}
			n_x.d += n_dx;
			n_y.d += n_dy;
			n_len--;
		}
	}
}

static void DebugMeshEnd( void )
{
	m_n_debugcoord = 0;
}

static int DebugMeshDisplay( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	if( keyboard_pressed_memory( KEYCODE_M ) )
	{
		m_b_debugmesh = !m_b_debugmesh;
	}
	if( m_b_debugmesh )
	{
		set_visible_area( 0, 1023, 0, 1023 );
		copybitmap( bitmap, debugmesh, 0, 0, 0, 0, cliprect, TRANSPARENCY_NONE, 0 );
	}
	m_b_debugclear = 1;
	return m_b_debugmesh;
}

static int DebugTextureDisplay( struct mame_bitmap *bitmap )
{
	UINT32 n_y;

	if( keyboard_pressed_memory( KEYCODE_V ) )
	{
		m_b_debugtexture = !m_b_debugtexture;
	}
	if( m_b_debugtexture )
	{
		if( keyboard_pressed_memory( KEYCODE_I ) )
		{
			m_n_debuginterleave++;
			if( m_n_debuginterleave == 2 )
			{
				m_n_debuginterleave = -1;
			}
			if( m_n_debuginterleave == -1 )
			{
				usrintf_showmessage_secs( 1, "interleave off" );
			}
			else if( m_n_debuginterleave == 0 )
			{
				usrintf_showmessage_secs( 1, "4 bit interleave" );
			}
			else if( m_n_debuginterleave == 1 )
			{
				usrintf_showmessage_secs( 1, "8 bit interleave" );
			}
		}
		set_visible_area( 0, 1023, 0, 1023 );

		for( n_y = 0; n_y < 1024; n_y++ )
		{
			int n_x;
			int n_xi;
			int n_yi;
			unsigned short p_n_interleave[ 1024 ];

			for( n_x = 0; n_x < 1024; n_x++ )
			{
				if( m_n_debuginterleave == 0 )
				{
					n_xi = ( n_x & ~0x3c ) + ( ( n_y << 2 ) & 0x3c );
					n_yi = ( n_y & ~0xf ) + ( ( n_x >> 2 ) & 0xf );
				}
				else if( m_n_debuginterleave == 1 )
				{
					n_xi = ( n_x & ~0x78 ) + ( ( n_x << 3 ) & 0x40 ) + ( ( n_y << 3 ) & 0x38 ); \
					n_yi = ( n_y & ~0x7 ) + ( ( n_x >> 4 ) & 0x7 ); \
				}
				else
				{
					n_xi = n_x;
					n_yi = n_y;
				}
				p_n_interleave[ n_x ] = m_p_p_vram[ n_yi ][ n_xi ];
			}

         const uint16_t *src = &p_n_interleave;
         int dy = bitmap->rowpixels;
         UINT16 *dst = (UINT16 *)bitmap->base + y * dy;
         int length = 1023;

         while (length--)
            *dst++ = *src++;
		}
	}
	return m_b_debugtexture;
}

#endif

static int psx_gpu_init( int n_width, int n_height )
{
	int n_line;
	int n_level;
	int n_level2;
	int n_shade;
	int n_shaded;

#if defined( MAME_DEBUG )
	DebugMeshInit();
#endif

	m_n_gpustatus = 0x14802000;
	m_n_drawmode = 0;
	m_n_gpuinfo = 0;
	m_n_gpu_buffer_offset = 0;

	m_n_vram_size = n_width * n_height;
	m_n_vram_width = n_width;
	m_n_vram_height = n_height;
	m_p_vram = auto_malloc( m_n_vram_size * 2 );
	if( m_p_vram == NULL )
	{
		return 1;
	}
	memset( m_p_vram, 0x00, m_n_vram_size * 2 );

	for( n_line = 0; n_line < 1024; n_line++ )
	{
		m_p_p_vram[ n_line ] = &m_p_vram[ ( n_line % n_height ) * n_width ];
	}

	for( n_level = 0; n_level < MAX_LEVEL; n_level++ )
	{
		for( n_shade = 0; n_shade < MAX_SHADE; n_shade++ )
		{
			/* shaded */
			n_shaded = ( n_level * n_shade ) / MID_SHADE;
			if( n_shaded > MAX_LEVEL - 1 )
			{
				n_shaded = MAX_LEVEL - 1;
			}
			m_p_n_redshade[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded;
			m_p_n_greenshade[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded << 5;
			m_p_n_blueshade[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded << 10;

			/* 1/4 x transparency */
			n_shaded = ( n_level * n_shade ) / MID_SHADE;
			n_shaded >>= 2;
			if( n_shaded > MAX_LEVEL - 1 )
			{
				n_shaded = MAX_LEVEL - 1;
			}
			m_p_n_f025[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded;

			/* 1/2 x transparency */
			n_shaded = ( n_level * n_shade ) / MID_SHADE;
			n_shaded >>= 1;
			if( n_shaded > MAX_LEVEL - 1 )
			{
				n_shaded = MAX_LEVEL - 1;
			}
			m_p_n_f05[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded;

			/* 1 x transparency */
			n_shaded = ( n_level * n_shade ) / MID_SHADE;
			if( n_shaded > MAX_LEVEL - 1 )
			{
				n_shaded = MAX_LEVEL - 1;
			}
			m_p_n_f1[ ( n_level * MAX_SHADE ) | n_shade ] = n_shaded;
		}
	}

	for( n_level = 0; n_level < 0x10000; n_level++ )
	{
		m_p_n_redlevel[ n_level ] = ( n_level & ( MAX_LEVEL - 1 ) ) * MAX_SHADE;
		m_p_n_greenlevel[ n_level ] = ( ( n_level >> 5 ) & ( MAX_LEVEL - 1 ) ) * MAX_SHADE;
		m_p_n_bluelevel[ n_level ] = ( ( n_level >> 10 ) & ( MAX_LEVEL - 1 ) ) * MAX_SHADE;

		/* 0.5 * background */
		m_p_n_redb05[ n_level ] = ( ( n_level & ( MAX_LEVEL - 1 ) ) / 2 ) * MAX_LEVEL;
		m_p_n_greenb05[ n_level ] = ( ( ( n_level >> 5 ) & ( MAX_LEVEL - 1 ) ) / 2 ) * MAX_LEVEL;
		m_p_n_blueb05[ n_level ] = ( ( ( n_level >> 10 ) & ( MAX_LEVEL - 1 ) ) / 2 ) * MAX_LEVEL;

		/* 1 * background */
		m_p_n_redb1[ n_level ] = ( n_level & ( MAX_LEVEL - 1 ) ) * MAX_LEVEL;
		m_p_n_greenb1[ n_level ] = ( ( n_level >> 5 ) & ( MAX_LEVEL - 1 ) ) * MAX_LEVEL;
		m_p_n_blueb1[ n_level ] = ( ( n_level >> 10 ) & ( MAX_LEVEL - 1 ) ) * MAX_LEVEL;

	}

	for( n_level = 0; n_level < MAX_LEVEL; n_level++ )
	{
		for( n_level2 = 0; n_level2 < MAX_LEVEL; n_level2++ )
		{
			/* add transparency */
			n_shaded = ( n_level + n_level2 );
			if( n_shaded > MAX_LEVEL - 1 )
			{
				n_shaded = MAX_LEVEL - 1;
			}
			m_p_n_redaddtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded;
			m_p_n_greenaddtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded << 5;
			m_p_n_blueaddtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded << 10;

			/* sub transparency */
			n_shaded = ( n_level - n_level2 );
			if( n_shaded < 0 )
			{
				n_shaded = 0;
			}
			m_p_n_redsubtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded;
			m_p_n_greensubtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded << 5;
			m_p_n_bluesubtrans[ ( n_level * MAX_LEVEL ) | n_level2 ] = n_shaded << 10;
		}
	}

	state_save_register_UINT8( "psx", 0, "m_packet", (UINT8 *)&m_packet, sizeof( m_packet ) );
	state_save_register_UINT16( "psx", 0, "m_p_vram", m_p_vram, m_n_vram_size );
	state_save_register_UINT32( "psx", 0, "m_n_gpu_buffer_offset", &m_n_gpu_buffer_offset, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_vramx", &m_n_vramx, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_vramy", &m_n_vramy, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_twy", &m_n_twy, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_twx", &m_n_twx, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_tww", &m_n_tww, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_twh", &m_n_twh, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_drawarea_x1", &m_n_drawarea_x1, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_drawarea_y1", &m_n_drawarea_y1, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_drawarea_x2", &m_n_drawarea_x2, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_drawarea_y2", &m_n_drawarea_y2, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_horiz_disstart", &m_n_horiz_disstart, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_horiz_disend", &m_n_horiz_disend, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_vert_disstart", &m_n_vert_disstart, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_vert_disend", &m_n_vert_disend, 1 );
	state_save_register_UINT32( "psx", 0, "m_b_reverseflag", &m_b_reverseflag, 1 );
	state_save_register_INT32( "psx", 0, "m_n_drawoffset_x", &m_n_drawoffset_x, 1 );
	state_save_register_INT32( "psx", 0, "m_n_drawoffset_y", &m_n_drawoffset_y, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_displaystartx", &m_n_displaystartx, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_displaystarty", &m_n_displaystarty, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_gpustatus", &m_n_gpustatus, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_gpuinfo", &m_n_gpuinfo, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_screenwidth", &m_n_screenwidth, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_screenheight", &m_n_screenheight, 1 );
	state_save_register_UINT32( "psx", 0, "m_n_drawmode", &m_n_drawmode, 1 );

	/* the drawing thread (System 10/11/12): VRAM complete in a save state */
	state_save_register_func_presave( psx_gpu_sync );
	state_save_register_func_postload( psx_gpu_sync );
	if( psx_gpu_thread_wanted )
	{
		psx_gq_start();
	}
	psx_gpu_thread_wanted = 0;	/* asked again by the next driver that wants it */

	return 0;
}

VIDEO_START( psx_type1_1024x1024 )
{
	m_n_gputype = 1;
	return psx_gpu_init( 1024, 1024 );
}

VIDEO_START( psx_type2_1024x512 )
{
	m_n_gputype = 2;
	return psx_gpu_init( 1024, 512 );
}

VIDEO_START( psx_type2_1024x1024 )
{
	m_n_gputype = 2;
	return psx_gpu_init( 1024, 1024 );
}

VIDEO_STOP( psx )
{
	psx_gq_stop();
}

/*
	Display line in VRAM (xBBBBBGGGGGRRRRR) to a direct colour bitmap
	(xRRRRRGGGGGBBBBB), for drivers with VIDEO_RGB_DIRECT: the libretro
	output then takes the bitmap as it is, without a palette lookup per
	pixel. NEON does 8 pixels at a time where the compiler targets it.
*/
#if defined( __ARM_NEON ) || defined( __ARM_NEON__ )
#include <arm_neon.h>
#endif

static void psx_line_to_rgb15( UINT16 *dst, const UINT16 *src, int n )
{
#if defined( __ARM_NEON ) || defined( __ARM_NEON__ )
	const uint16x8_t m5 = vdupq_n_u16( 0x001f );
	const uint16x8_t m_g = vdupq_n_u16( 0x03e0 );
	for( ; n >= 8; n -= 8, src += 8, dst += 8 )
	{
		uint16x8_t v = vld1q_u16( src );
		uint16x8_t r = vshlq_n_u16( vandq_u16( v, m5 ), 10 );
		uint16x8_t g = vandq_u16( v, m_g );
		uint16x8_t b = vandq_u16( vshrq_n_u16( v, 10 ), m5 );
		vst1q_u16( dst, vorrq_u16( vorrq_u16( r, g ), b ) );
	}
#endif
	while( n-- > 0 )
	{
		UINT16 v = *src++;
		*dst++ = ( ( v & 0x1f ) << 10 ) | ( v & 0x3e0 ) | ( ( v >> 10 ) & 0x1f );
	}
}

VIDEO_UPDATE( psx )
{
	int direct = ( Machine->drv->video_attributes & VIDEO_RGB_DIRECT ) != 0;
	UINT32 n_x;
	UINT32 n_y;

	psx_gpu_sync();

#if defined( MAME_DEBUG )
	if( DebugMeshDisplay( bitmap, cliprect ) )
	{
		return;
	}
	if( DebugTextureDisplay( bitmap ) )
	{
		return;
	}
#if 0
	if( keyboard_pressed_memory( KEYCODE_I ) )
	{
		m_n_debugskip++;
		if( m_n_debugskip > 13 )
		{
			m_n_debugskip = 0;
		}
		usrintf_showmessage_secs( 1, "debug skip %d", m_n_debugskip );
	}
	if( keyboard_pressed_memory( KEYCODE_D ) )
	{
		FILE *f;
		int n_x;
		f = fopen( "dump.txt", "w" );
		for( n_y = 256; n_y < 512; n_y++ )
		{
			for( n_x = 640; n_x < 1024; n_x++ )
			{
				fprintf( f, "%04u,%04u = %04x\n", n_y, n_x, m_p_p_vram[ n_y ][ n_x ] );
			}
		}
		fclose( f );
	}
	if( keyboard_pressed_memory( KEYCODE_S ) )
	{
		FILE *f;
		usrintf_showmessage_secs( 1, "saving..." );
		f = fopen( "VRAM.BIN", "wb" );
		for( n_y = 0; n_y < 1024; n_y++ )
		{
			fwrite( m_p_p_vram[ n_y ], 1024 * 2, 1, f );
		}
		fclose( f );
	}
	if( keyboard_pressed_memory( KEYCODE_L ) )
	{
		FILE *f;
		usrintf_showmessage_secs( 1, "loading..." );
		f = fopen( "VRAM.BIN", "rb" );
		for( n_y = 0; n_y < 1024; n_y++ )
		{
			fread( m_p_p_vram[ n_y ], 1024 * 2, 1, f );
		}
		fclose( f );
	}
#endif
#endif

	set_visible_area( 0, m_n_screenwidth - 1, 0, m_n_screenheight - 1 );

	if( ( m_n_gpustatus & ( 1 << 0x17 ) ) != 0 )
	{
		/* todo: only draw to necessary area */
      int sy, ey, sx, ex, y;
      sy = cliprect->min_y;
      ey = cliprect->max_y;
      sx = cliprect->min_x;
      ex = cliprect->max_x;

      for (y = sy;y <= ey;y++)
         memset(((UINT16 *)bitmap->line[y]) + sx, 0, (ex-sx+1)*2);
	}
	else
	{
		/* todo: clear border */
		if( m_b_reverseflag )
		{
			n_x = ( 1023 - m_n_displaystartx );
			/* todo: make this flip the screen, in the meantime.. */
			n_x -= ( m_n_screenwidth - 1 );
		}
		else
		{
			n_x = m_n_displaystartx;
		}

		if( ( m_n_gpustatus & ( 1L << 0x15 ) ) != 0 )
		{
			/* 24-bit display mode (MDEC movies): 3 bytes per pixel in VRAM,
			   reduced to the 15-bit palette used by the bitmap */
			for( n_y = 0; n_y < m_n_screenheight; n_y++ )
			{
				const UINT16 *src = m_p_p_vram[ ( n_y + m_n_displaystarty ) & 1023 ];
				UINT16 *dst = (UINT16 *)bitmap->base + n_y * bitmap->rowpixels;
				UINT32 n_byte = n_x * 2;
				int length = m_n_screenwidth;

				while( length-- )
				{
					UINT32 n_r, n_g, n_b;
					n_r = ( src[ ( n_byte >> 1 ) & 1023 ] >> ( ( n_byte & 1 ) * 8 ) ) & 0xff; n_byte++;
					n_g = ( src[ ( n_byte >> 1 ) & 1023 ] >> ( ( n_byte & 1 ) * 8 ) ) & 0xff; n_byte++;
					n_b = ( src[ ( n_byte >> 1 ) & 1023 ] >> ( ( n_byte & 1 ) * 8 ) ) & 0xff; n_byte++;
					if( direct )
						*dst++ = ( ( n_r >> 3 ) << 10 ) | ( ( n_g >> 3 ) << 5 ) | ( n_b >> 3 );
					else
						*dst++ = ( ( n_b >> 3 ) << 10 ) | ( ( n_g >> 3 ) << 5 ) | ( n_r >> 3 );
				}
			}
		}
		else
		for( n_y = 0; n_y < m_n_screenheight; n_y++ )
      {
         const uint16_t *src = (m_p_p_vram[ n_y + m_n_displaystarty ] + n_x);
         int dy = bitmap->rowpixels;
         UINT16 *dst = (UINT16 *)bitmap->base + n_y * dy;
         int length = m_n_screenwidth;

         if( direct )
            psx_line_to_rgb15( dst, src, length );
         else
            while (length--)
               *dst++ = *src++;
      }
	}
}

/*
type 1
f  e| d| c  b| a  9| 8  7| 6  5| 4| 3  2  1  0
    |ti|     |   tp|  abr|   ty|  |         tx
*/

/*
type 2
f  e  d  c| b| a  9| 8  7| 6  5| 4| 3  2  1  0
          |ty|     |   tp|  abr|ty|         tx
*/

/*
	Drawing on a second thread (psx_gpu_set_thread; System 10/11/12).

	psx_gpu_write parses the commands and keeps every register on the
	emulation thread, as before. Each primitive is drawn with a copy of
	its packet and of the drawing state of that moment (struct psx_draw):
	at once, or in order by the drawing thread. VIDEO_UPDATE, a transfer
	from VRAM to the CPU, the VBLANK and the save states wait until
	everything queued has been drawn, so VRAM and what the emulation sees
	are the same with and without the thread.

	The drawing code below reads the state through psx_d.
*/
struct psx_draw
{
	psx_gpu_packet packet;
	UINT32 twy, twx, twh, tww;
	UINT32 drawarea_x1, drawarea_y1, drawarea_x2, drawarea_y2;
	INT32 drawoffset_x, drawoffset_y;
	UINT32 drawmode;
	UINT32 gpustatus;
};

static struct psx_draw *psx_d;

#define m_packet ( psx_d->packet )
#define m_n_twy ( psx_d->twy )
#define m_n_twx ( psx_d->twx )
#define m_n_twh ( psx_d->twh )
#define m_n_tww ( psx_d->tww )
#define m_n_drawarea_x1 ( psx_d->drawarea_x1 )
#define m_n_drawarea_y1 ( psx_d->drawarea_y1 )
#define m_n_drawarea_x2 ( psx_d->drawarea_x2 )
#define m_n_drawarea_y2 ( psx_d->drawarea_y2 )
#define m_n_drawoffset_x ( psx_d->drawoffset_x )
#define m_n_drawoffset_y ( psx_d->drawoffset_y )
#define m_n_drawmode ( psx_d->drawmode )
#define m_n_gpustatus ( psx_d->gpustatus )

#define TRANSPARENCYSETUP \
	p_n_f = m_p_n_f1; \
	p_n_redb = m_p_n_redb1; \
	p_n_greenb = m_p_n_greenb1; \
	p_n_blueb = m_p_n_blueb1; \
	p_n_redtrans = m_p_n_redaddtrans; \
	p_n_greentrans = m_p_n_greenaddtrans; \
	p_n_bluetrans = m_p_n_blueaddtrans; \
 \
	switch( n_cmd & 0x02 ) \
	{ \
	case 0x02: \
		switch( n_abr & 0x03 ) \
		{ \
		case 0x00: \
			p_n_f = m_p_n_f05; \
			p_n_redb = m_p_n_redb05; \
			p_n_greenb = m_p_n_greenb05; \
			p_n_blueb = m_p_n_blueb05; \
			p_n_redtrans = m_p_n_redaddtrans; \
			p_n_greentrans = m_p_n_greenaddtrans; \
			p_n_bluetrans = m_p_n_blueaddtrans; \
			verboselog( 2, "Transparency Mode: 0.5*B + 0.5*F\n" ); \
			break; \
		case 0x01: \
			p_n_f = m_p_n_f1; \
			p_n_redb = m_p_n_redb1; \
			p_n_greenb = m_p_n_greenb1; \
			p_n_blueb = m_p_n_blueb1; \
			p_n_redtrans = m_p_n_redaddtrans; \
			p_n_greentrans = m_p_n_greenaddtrans; \
			p_n_bluetrans = m_p_n_blueaddtrans; \
			verboselog( 2, "Transparency Mode: 1.0*B + 1.0*F\n" ); \
			break; \
		case 0x02: \
			p_n_f = m_p_n_f1; \
			p_n_redb = m_p_n_redb1; \
			p_n_greenb = m_p_n_greenb1; \
			p_n_blueb = m_p_n_blueb1; \
			p_n_redtrans = m_p_n_redsubtrans; \
			p_n_greentrans = m_p_n_greensubtrans; \
			p_n_bluetrans = m_p_n_bluesubtrans; \
			verboselog( 2, "Transparency Mode: 1.0*B - 1.0*F\n" ); \
			break; \
		case 0x03: \
			p_n_f = m_p_n_f025; \
			p_n_redb = m_p_n_redb1; \
			p_n_greenb = m_p_n_greenb1; \
			p_n_blueb = m_p_n_blueb1; \
			p_n_redtrans = m_p_n_redaddtrans; \
			p_n_greentrans = m_p_n_greenaddtrans; \
			p_n_bluetrans = m_p_n_blueaddtrans; \
			verboselog( 2, "Transparency Mode: 1.0*B + 0.25*F\n" ); \
			break; \
		} \
		break; \
	}

#define SOLIDSETUP( DRAWMODE ) \
	if( m_n_gputype == 2 ) \
	{ \
		n_abr = ( DRAWMODE & 0x60 ) >> 5; \
	} \
	else \
	{ \
		n_abr = ( DRAWMODE & 0x180 ) >> 7; \
	} \
	TRANSPARENCYSETUP

#define TEXTURESETUP( DRAWMODE ) \
	p_clut = m_p_p_vram[ n_cluty ] + n_clutx; \
	if( m_n_gputype == 2 ) \
	{ \
		if( m_n_drawmode != DRAWMODE ) \
		{ \
			m_n_drawmode = DRAWMODE; \
			m_n_gpustatus = ( m_n_gpustatus & 0xfffff800 ) | ( m_n_drawmode & 0x7ff ); \
		} \
		n_tx = ( DRAWMODE & 0x0f ) << 6; \
		n_ty = ( ( DRAWMODE & 0x10 ) << 4 ) | \
			( ( DRAWMODE & 0x800 ) >> 2 ); \
		n_abr = ( DRAWMODE & 0x60 ) >> 5; \
		n_tp = ( DRAWMODE & 0x180 ) >> 7; \
		n_ti = 0; \
	} \
	else \
	{ \
		if( m_n_drawmode != DRAWMODE ) \
		{ \
			m_n_drawmode = DRAWMODE; \
			m_n_gpustatus = ( m_n_gpustatus & 0xffffe000 ) | ( m_n_drawmode & 0x1fff ); \
		} \
		n_tx = ( DRAWMODE & 0x0f ) << 6; \
		n_ty = ( ( DRAWMODE & 0x60 ) << 3 ); \
		n_abr = ( DRAWMODE & 0x180 ) >> 7; \
		n_tp = ( DRAWMODE & 0x600 ) >> 9; \
		n_ti = ( DRAWMODE & 0x2000 ) >> 13; \
	} \
 \
	switch( n_tp ) \
	{ \
	case 0: \
		n_tx += m_n_twx >> 2; \
		n_ty += m_n_twy >> 2; \
		break; \
	case 1: \
		n_tx += m_n_twx >> 1; \
		n_ty += m_n_twy >> 1; \
		break; \
	case 2: \
		n_tx += m_n_twx >> 0; \
		n_ty += m_n_twy >> 0; \
		break; \
	} \
	TRANSPARENCYSETUP

#define FLATPOLYGONUPDATE
#define FLATRECTANGEUPDATE
#define GOURAUDPOLYGONUPDATE \
	n_r.d += n_dr; \
	n_g.d += n_dg; \
	n_b.d += n_db;

#define SOLIDFILL( PIXELUPDATE ) \
	if( n_distance > ( (INT32)m_n_drawarea_x2 - n_x ) + 1 ) \
	{ \
		n_distance = ( m_n_drawarea_x2 - n_x ) + 1; \
	} \
	p_vram = m_p_p_vram[ n_y ] + n_x; \
 \
	switch( n_cmd & 0x02 ) \
	{ \
	case 0x00: \
		/* transparency off */ \
		while( n_distance > 0 ) \
		{ \
			*( p_vram ) = m_p_n_redshade[ MID_LEVEL | n_r.w.h ] | \
				m_p_n_greenshade[ MID_LEVEL | n_g.w.h ] | \
				m_p_n_blueshade[ MID_LEVEL | n_b.w.h ]; \
			p_vram++; \
			PIXELUPDATE \
			n_distance--; \
		} \
		break; \
	case 0x02: \
		/* transparency on */ \
		while( n_distance > 0 ) \
		{ \
			*( p_vram ) = p_n_redtrans[ p_n_f[ MID_LEVEL | n_r.w.h ] | p_n_redb[ *( p_vram ) ] ] | \
				p_n_greentrans[ p_n_f[ MID_LEVEL | n_g.w.h ] | p_n_greenb[ *( p_vram ) ] ] | \
				p_n_bluetrans[ p_n_f[ MID_LEVEL | n_b.w.h ] | p_n_blueb[ *( p_vram ) ] ]; \
			p_vram++; \
			PIXELUPDATE \
			n_distance--; \
		} \
		break; \
	} \

#define FLATTEXTUREDPOLYGONUPDATE \
	n_u.d += n_du; \
	n_v.d += n_dv;

#define GOURAUDTEXTUREDPOLYGONUPDATE \
	n_r.d += n_dr; \
	n_g.d += n_dg; \
	n_b.d += n_db; \
	n_u.d += n_du; \
	n_v.d += n_dv;

#define FLATTEXTUREDRECTANGLEUPDATE \
	n_u++;

#define TEXTURE4BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		n_bgr = p_clut[ ( *( m_p_p_vram[ n_ty + TXV ] + n_tx + ( TXU >> 2 ) ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ];

#define TEXTURE8BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		n_bgr = p_clut[ ( *( m_p_p_vram[ n_ty + TXV ] + n_tx + ( TXU >> 1 ) ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff ];

#define TEXTURE15BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		n_bgr = *( m_p_p_vram[ n_ty + TXV ] + n_tx + TXU );

#define TEXTUREWINDOW4BIT( TXV, TXU ) TEXTURE4BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )
#define TEXTUREWINDOW8BIT( TXV, TXU ) TEXTURE8BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )
#define TEXTUREWINDOW15BIT( TXV, TXU ) TEXTURE15BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )

#define TEXTUREINTERLEAVED4BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		int n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c ); \
		int n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf ); \
		n_bgr = p_clut[ ( *( m_p_p_vram[ n_ty + n_yi ] + n_tx + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ];

#define TEXTUREINTERLEAVED8BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		int n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 ); \
		int n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 ); \
		n_bgr = p_clut[ ( *( m_p_p_vram[ n_ty + n_yi ] + n_tx + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff ];

#define TEXTUREINTERLEAVED15BIT( TXV, TXU ) \
	while( n_distance > 0 ) \
	{ \
		int n_xi = TXU; \
		int n_yi = TXV; \
		n_bgr = *( m_p_p_vram[ n_ty + n_yi ] + n_tx + n_xi );

#define TEXTUREWINDOWINTERLEAVED4BIT( TXV, TXU ) TEXTUREINTERLEAVED4BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )
#define TEXTUREWINDOWINTERLEAVED8BIT( TXV, TXU ) TEXTUREINTERLEAVED8BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )
#define TEXTUREWINDOWINTERLEAVED15BIT( TXV, TXU ) TEXTUREINTERLEAVED15BIT( ( TXV % m_n_twh ), ( TXU % m_n_tww ) )

#define SHADEDPIXEL( PIXELUPDATE ) \
		if( n_bgr != 0 ) \
		{ \
			*( p_vram ) = m_p_n_redshade[ m_p_n_redlevel[ n_bgr ] | n_r.w.h ] | \
				m_p_n_greenshade[ m_p_n_greenlevel[ n_bgr ] | n_g.w.h ] | \
				m_p_n_blueshade[ m_p_n_bluelevel[ n_bgr ] | n_b.w.h ]; \
		} \
		p_vram++; \
		PIXELUPDATE \
		n_distance--; \
	}

#define TRANSPARENTPIXEL( PIXELUPDATE ) \
		if( n_bgr != 0 ) \
		{ \
			if( ( n_bgr & 0x8000 ) != 0 ) \
			{ \
				*( p_vram ) = p_n_redtrans[ p_n_f[ m_p_n_redlevel[ n_bgr ] | n_r.w.h ] | p_n_redb[ *( p_vram ) ] ] | \
					p_n_greentrans[ p_n_f[ m_p_n_greenlevel[ n_bgr ] | n_g.w.h ] | p_n_greenb[ *( p_vram ) ] ] | \
					p_n_bluetrans[ p_n_f[ m_p_n_bluelevel[ n_bgr ] | n_b.w.h ] | p_n_blueb[ *( p_vram ) ] ]; \
			} \
			else \
			{ \
				*( p_vram ) = m_p_n_redshade[ m_p_n_redlevel[ n_bgr ] | n_r.w.h ] | \
					m_p_n_greenshade[ m_p_n_greenlevel[ n_bgr ] | n_g.w.h ] | \
					m_p_n_blueshade[ m_p_n_bluelevel[ n_bgr ] | n_b.w.h ]; \
			} \
		} \
		p_vram++; \
		PIXELUPDATE \
		n_distance--; \
	}

/*
	Textured span, the common case (standard texture page, not
	interleaved): what TEXTUREFILL does for one span, pixel for pixel the
	same result. With NEON, 8 pixels at a time. The texel addresses of the
	8 pixels (texture window included) are worked out together and the
	texels loaded one per lane; 4-bit CLUT entries come from a byte table
	lookup. Shading, semi-transparency, the transparent colour 0 and the
	store are done on all 8 together, with the arithmetic the shade and
	transparency tables hold. A shorter last block keeps its unused lanes
	transparent, so they store back what they read. Pixels the arithmetic
	could not match (a shade above 255) go through the tables as before.
	A span whose texture rows or CLUT row are the row it draws to is not
	taken (the original reads back what it has just drawn), nor are
	interleaved textures; the caller then uses TEXTUREFILL. Untextured
	spans (SOLIDFILL) take the same path with a constant texel.

	u, v, r, g, b are 16.16 values stepped per pixel; TXU/TXV are their
	high 16 bits masked with umask/vmask (0xff when the caller's
	coordinates are bytes). Returns 1 when the span was drawn.
*/
#if defined( __ARM_NEON ) || defined( __ARM_NEON__ )
#define PSX_TEXSPAN			1
#else
#define PSX_TEXSPAN			0
#endif

#if PSX_TEXSPAN

struct psx_texspan
{
	UINT32 u, v, r, g, b;
	INT32 du, dv, dr, dg, db;
	UINT32 umask, vmask;
	UINT32 tp, tx, ty, abr;
	int semi;
	const UINT16 *clut;
};

#ifdef PSX_TEXSPAN_VERIFY
static INLINE UINT16 psx_texspan_texel( const struct psx_texspan *s, int win, UINT32 u, UINT32 v )
{
	UINT32 n_u = ( u >> 16 ) & s->umask;
	UINT32 n_v = ( v >> 16 ) & s->vmask;
	if( win )
	{
		n_u %= m_n_tww;
		n_v %= m_n_twh;
	}
	switch( s->tp )
	{
	case 0:
		return s->clut[ ( *( m_p_p_vram[ s->ty + n_v ] + s->tx + ( n_u >> 2 ) ) >> ( ( n_u & 0x03 ) << 2 ) ) & 0x0f ];
	case 1:
		return s->clut[ ( *( m_p_p_vram[ s->ty + n_v ] + s->tx + ( n_u >> 1 ) ) >> ( ( n_u & 0x01 ) << 3 ) ) & 0xff ];
	default:
		return *( m_p_p_vram[ s->ty + n_v ] + s->tx + n_u );
	}
}
#endif

/* one pixel through the tables, as SHADEDPIXEL / TRANSPARENTPIXEL */
static INLINE void psx_texspan_pixel( const struct psx_texspan *s, UINT16 *p_vram, UINT16 n_bgr, UINT32 r, UINT32 g, UINT32 b )
{
	UINT16 *p_n_f = m_p_n_f1, *p_n_redb = m_p_n_redb1, *p_n_greenb = m_p_n_greenb1, *p_n_blueb = m_p_n_blueb1;
	UINT16 *p_n_redtrans = m_p_n_redaddtrans, *p_n_greentrans = m_p_n_greenaddtrans, *p_n_bluetrans = m_p_n_blueaddtrans;
	UINT16 rh = r >> 16, gh = g >> 16, bh = b >> 16;

	if( n_bgr == 0 )
	{
		return;
	}
	if( s->semi && ( n_bgr & 0x8000 ) != 0 )
	{
		switch( s->abr )
		{
		case 0x00:
			p_n_f = m_p_n_f05; p_n_redb = m_p_n_redb05; p_n_greenb = m_p_n_greenb05; p_n_blueb = m_p_n_blueb05;
			break;
		case 0x02:
			p_n_redtrans = m_p_n_redsubtrans; p_n_greentrans = m_p_n_greensubtrans; p_n_bluetrans = m_p_n_bluesubtrans;
			break;
		case 0x03:
			p_n_f = m_p_n_f025;
			break;
		}
		*( p_vram ) = p_n_redtrans[ p_n_f[ m_p_n_redlevel[ n_bgr ] | rh ] | p_n_redb[ *( p_vram ) ] ] |
			p_n_greentrans[ p_n_f[ m_p_n_greenlevel[ n_bgr ] | gh ] | p_n_greenb[ *( p_vram ) ] ] |
			p_n_bluetrans[ p_n_f[ m_p_n_bluelevel[ n_bgr ] | bh ] | p_n_blueb[ *( p_vram ) ] ];
	}
	else
	{
		*( p_vram ) = m_p_n_redshade[ m_p_n_redlevel[ n_bgr ] | rh ] |
			m_p_n_greenshade[ m_p_n_greenlevel[ n_bgr ] | gh ] |
			m_p_n_blueshade[ m_p_n_bluelevel[ n_bgr ] | bh ];
	}
}

/* does the span read the row it draws to (texture rows, CLUT)? */
static int psx_texspan_overlap( const struct psx_texspan *s, int win, const UINT16 *p_vram, int n )
{
	const UINT32 n_row = ( p_vram - m_p_vram ) / m_n_vram_width;
	const UINT32 n_height = m_n_vram_height;
	const UINT16 *row = m_p_vram + n_row * m_n_vram_width;
	INT64 v0 = (INT64)s->v, v1 = (INT64)s->v + (INT64)s->dv * ( n - 1 );
	UINT32 vmin, vmax;

	if( s->tp != 2 && s->clut >= row && s->clut < row + m_n_vram_width )
	{
		return 1;
	}
	if( win )
	{
		vmin = 0;
		vmax = m_n_twh - 1;
	}
	else
	{
		/* v stays within one 16-bit step range: the masked rows are contiguous */
		if( v0 < 0 || v1 < 0 || v0 > 0xffffffffLL || v1 > 0xffffffffLL )
		{
			return 1;
		}
		vmin = (UINT32)( ( v0 < v1 ? v0 : v1 ) >> 16 );
		vmax = (UINT32)( ( v0 < v1 ? v1 : v0 ) >> 16 );
		if( ( vmin & ~s->vmask ) != ( vmax & ~s->vmask ) )
		{
			return 1;
		}
		vmin &= s->vmask;
		vmax &= s->vmask;
	}
	if( vmax - vmin > 255 || s->ty + vmax >= 1024 )
	{
		return 1;
	}
	/* texture line l is vram row l % n_height: is the drawn row among ty + vmin .. ty + vmax? */
	if( vmax - vmin >= n_height )
	{
		return 1;
	}
	return ( n_row + n_height - ( s->ty + vmin ) % n_height ) % n_height <= vmax - vmin;
}

#define PSX_TS_ALWAYS static __inline__ __attribute__(( always_inline ))

/* x % d for x < 65536 and 2 <= d <= 256, with m = psx_ts_recip( d ) */
static UINT32 psx_ts_recip_d[ 2 ], psx_ts_recip_m[ 2 ];
PSX_TS_ALWAYS UINT32 psx_ts_recip( int k, UINT32 d )
{
	if( psx_ts_recip_d[ k ] != d )
	{
		psx_ts_recip_d[ k ] = d;
		psx_ts_recip_m[ k ] = (UINT32)( 0xffffffffULL / d ) + 1;
	}
	return psx_ts_recip_m[ k ];
}
PSX_TS_ALWAYS UINT32 psx_ts_mod( UINT32 x, UINT32 d, UINT32 m )
{
	return x - d * (UINT32)( ( (UINT64)x * m ) >> 32 );
}

/* psx_ts_mod on 8 lanes */
PSX_TS_ALWAYS uint16x8_t psx_ts_vmod( uint16x8_t x, UINT32 d, UINT32 m )
{
	const uint32x2_t mv = vdup_n_u32( m );
	const uint32x4_t lo = vmovl_u16( vget_low_u16( x ) ), hi = vmovl_u16( vget_high_u16( x ) );
	const uint32x4_t qlo = vcombine_u32( vshrn_n_u64( vmull_u32( vget_low_u32( lo ), mv ), 32 ), vshrn_n_u64( vmull_u32( vget_high_u32( lo ), mv ), 32 ) );
	const uint32x4_t qhi = vcombine_u32( vshrn_n_u64( vmull_u32( vget_low_u32( hi ), mv ), 32 ), vshrn_n_u64( vmull_u32( vget_high_u32( hi ), mv ), 32 ) );
	return vmlsq_n_u16( x, vcombine_u16( vmovn_u32( qlo ), vmovn_u32( qhi ) ), (UINT16)d );
}

/* texel n_u of a texture row (tx already added) */
PSX_TS_ALWAYS UINT16 psx_ts_texel( const UINT16 *clut, const UINT16 *row, const int tp, UINT32 n_u )
{
	switch( tp )
	{
	case 0:
		return clut[ ( row[ n_u >> 2 ] >> ( ( n_u & 0x03 ) << 2 ) ) & 0x0f ];
	case 1:
		return clut[ ( row[ n_u >> 1 ] >> ( ( n_u & 0x01 ) << 3 ) ) & 0xff ];
	default:
		return row[ n_u ];
	}
}

/* largest lane */
PSX_TS_ALWAYS UINT16 psx_texspan_max( uint16x8_t x )
{
#if defined( __aarch64__ )
	return vmaxvq_u16( x );
#else
	uint16x4_t m = vmax_u16( vget_low_u16( x ), vget_high_u16( x ) );
	m = vpmax_u16( m, m );
	m = vpmax_u16( m, m );
	return vget_lane_u16( m, 0 );
#endif
}

/* 8 lanes of x + i * dx ( i = 0..7 ), high 16 bits */
PSX_TS_ALWAYS uint16x8_t psx_ts_hi16( uint32x4_t lo, uint32x4_t hi )
{
	return vcombine_u16( vshrn_n_u32( lo, 16 ), vshrn_n_u32( hi, 16 ) );
}

/*
	The span, 8 pixels a block. tp, win (texture window) and fixrow (dv is
	0 and no window: one texture row for the whole span) are constants in
	each copy of this function. The last block may hold fewer than 8
	pixels: its unused lanes have texel 0 (transparent), so they store
	back what they read; when 8 lanes would run past the end of vram the
	last pixels go through the tables instead.
*/
PSX_TS_ALWAYS void psx_ts_span( const struct psx_texspan *s, UINT16 *p_vram, int n, const int tp, const int win, const int fixrow )
{
	static const UINT32 a_lane[ 8 ] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	const uint32x4_t lane_lo = vld1q_u32( a_lane ), lane_hi = vld1q_u32( a_lane + 4 );
	const uint16x8_t m5 = vdupq_n_u16( 0x1f );
	const uint16x8_t c31 = vdupq_n_u16( 31 );
	const uint16x8_t zero = vdupq_n_u16( 0 );
	const uint16x8_t bit15 = vdupq_n_u16( 0x8000 );
	const UINT16 *vram_end = m_p_vram + m_n_vram_size;
	const UINT16 *clut = s->clut;
	const UINT32 umask = s->umask, vmask = s->vmask;
	const INT32 du = s->du, dv = s->dv;
	const UINT32 tww = m_n_tww, twh = m_n_twh;
	const UINT32 mw = win ? psx_ts_recip( 0, tww ) : 0, mh = win ? psx_ts_recip( 1, twh ) : 0;
	UINT32 u = s->u, v = s->v, r = s->r, g = s->g, b = s->b;
	uint32x4_t r_lo = vmlaq_n_u32( vdupq_n_u32( r ), lane_lo, s->dr ), r_hi = vmlaq_n_u32( vdupq_n_u32( r ), lane_hi, s->dr );
	uint32x4_t g_lo = vmlaq_n_u32( vdupq_n_u32( g ), lane_lo, s->dg ), g_hi = vmlaq_n_u32( vdupq_n_u32( g ), lane_hi, s->dg );
	uint32x4_t b_lo = vmlaq_n_u32( vdupq_n_u32( b ), lane_lo, s->db ), b_hi = vmlaq_n_u32( vdupq_n_u32( b ), lane_hi, s->db );
	const uint32x4_t r_step = vdupq_n_u32( (UINT32)s->dr * 8 ), g_step = vdupq_n_u32( (UINT32)s->dg * 8 ), b_step = vdupq_n_u32( (UINT32)s->db * 8 );
	const UINT16 *row = NULL;
	/* vram row of texture line l: ( l & hmask ) x vram_w, when the height is a power of 2 */
	const UINT32 vram_h = m_n_vram_height;
#if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	const int vecrows = ( vram_h & ( vram_h - 1 ) ) == 0 && m_n_vram_width <= 0xffff;
#else
	const int vecrows = 0;
#endif
	const uint16x8_t hmask_v = vdupq_n_u16( vram_h - 1 ), ty_v = vdupq_n_u16( s->ty );
	const uint16x8_t umask_v = vdupq_n_u16( umask ), vmask_v = vdupq_n_u16( vmask );
	const uint32x4_t tx_v = vdupq_n_u32( s->tx );
	const UINT16 vram_w = m_n_vram_width;
	uint8x8x2_t clut_lo, clut_hi;
	int i;

	if( fixrow )
	{
		row = m_p_p_vram[ s->ty + ( ( v >> 16 ) & vmask ) ] + s->tx;
	}
	if( tp == 0 )
	{
		/* the 16 CLUT entries, low bytes and high bytes */
		const uint8x16x2_t c = vld2q_u8( (const UINT8 *)clut );
		clut_lo.val[ 0 ] = vget_low_u8( c.val[ 0 ] ); clut_lo.val[ 1 ] = vget_high_u8( c.val[ 0 ] );
		clut_hi.val[ 0 ] = vget_low_u8( c.val[ 1 ] ); clut_hi.val[ 1 ] = vget_high_u8( c.val[ 1 ] );
	}
	else
	{
		clut_lo.val[ 0 ] = clut_lo.val[ 1 ] = clut_hi.val[ 0 ] = clut_hi.val[ 1 ] = vdup_n_u8( 0 );
	}

	for( ; n > 0; n -= 8, p_vram += 8 )
	{
		UINT16 a_bgr[ 8 ];
		const int m = n < 8 ? n : 8;
		UINT32 r0 = r, g0 = g, b0 = b;
		uint16x8_t bgr, sr, sg, sb, cr, cg, cb, rr, rg, rb, out, dst;

		if( m < 8 && p_vram + 8 > vram_end )
		{
			/* the last pixels of vram */
			for( i = 0; i < m; i++ )
			{
				UINT32 n_u = ( u >> 16 ) & umask, n_v = ( v >> 16 ) & vmask;
				if( win )
				{
					n_u = psx_ts_mod( n_u, tww, mw );
					n_v = psx_ts_mod( n_v, twh, mh );
				}
				psx_texspan_pixel( s, p_vram + i, tp == 3 ? ( s->semi ? 0xc210 : 0x4210 ) : psx_ts_texel( clut, fixrow ? row : m_p_p_vram[ s->ty + n_v ] + s->tx, tp, n_u ), r, g, b );
				u += du; v += dv; r += s->dr; g += s->dg; b += s->db;
			}
			return;
		}

		/* the texels */
		if( tp == 3 )
		{
			/* untextured: level 16 on each channel, the semi-transparency bit when it is on */
			bgr = vdupq_n_u16( s->semi ? 0xc210 : 0x4210 );
			if( m < 8 )
			{
				bgr = vandq_u16( bgr, vcltq_u16( vcombine_u16( vmovn_u32( lane_lo ), vmovn_u32( lane_hi ) ), vdupq_n_u16( m ) ) );
			}
		}
		else if( fixrow && du == 0x10000 && m == 8 && tp != 0 && ( ( u >> 16 ) & umask ) + 7 <= umask )
		{
			/* one row, consecutive texels */
			const UINT32 n_u = ( u >> 16 ) & umask;
			if( tp == 2 )
			{
				bgr = vld1q_u16( row + n_u );
			}
			else
			{
#if defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				const UINT8 *p8 = (const UINT8 *)row + n_u;
				for( i = 0; i < 8; i++ )
				{
					a_bgr[ i ] = clut[ p8[ i ] ];
				}
#else
				for( i = 0; i < 8; i++ )
				{
					a_bgr[ i ] = psx_ts_texel( clut, row, tp, n_u + i );
				}
#endif
				bgr = vld1q_u16( a_bgr );
			}
			u += 0x80000;
			v += dv * 8;
		}
		else if( vecrows )
		{
			/* texel addresses on all 8 lanes, then one load per lane */
			uint16x8_t n_u = vandq_u16( psx_ts_hi16( vmlaq_n_u32( vdupq_n_u32( u ), lane_lo, du ), vmlaq_n_u32( vdupq_n_u32( u ), lane_hi, du ) ), umask_v );
			uint16x8_t n_v = vandq_u16( psx_ts_hi16( vmlaq_n_u32( vdupq_n_u32( v ), lane_lo, dv ), vmlaq_n_u32( vdupq_n_u32( v ), lane_hi, dv ) ), vmask_v );
			if( win )
			{
				n_u = psx_ts_vmod( n_u, tww, mw );
				n_v = psx_ts_vmod( n_v, twh, mh );
			}
			{
			const uint16x8_t line = vandq_u16( vaddq_u16( n_v, ty_v ), hmask_v );
			const uint16x8_t n_w = tp == 0 ? vshrq_n_u16( n_u, 2 ) : tp == 1 ? vshrq_n_u16( n_u, 1 ) : n_u;
			UINT32 a_idx[ 8 ];
			UINT16 a_w[ 8 ];
			uint16x8_t w;
			vst1q_u32( a_idx, vmlal_n_u16( vaddw_u16( tx_v, vget_low_u16( n_w ) ), vget_low_u16( line ), vram_w ) );
			vst1q_u32( a_idx + 4, vmlal_n_u16( vaddw_u16( tx_v, vget_high_u16( n_w ) ), vget_high_u16( line ), vram_w ) );
			if( m == 8 )
			{
				for( i = 0; i < 8; i++ )
				{
					a_w[ i ] = m_p_vram[ a_idx[ i ] ];
				}
			}
			else
			{
				for( i = 0; i < m; i++ )
				{
					a_w[ i ] = m_p_vram[ a_idx[ i ] ];
				}
				for( ; i < 8; i++ )
				{
					a_w[ i ] = 0;
				}
			}
			w = vld1q_u16( a_w );
			if( tp == 0 )
			{
				/* 4-bit: the 16 CLUT entries are a byte table lookup */
				const int16x8_t sh = vnegq_s16( vreinterpretq_s16_u16( vshlq_n_u16( vandq_u16( n_u, vdupq_n_u16( 3 ) ), 2 ) ) );
				const uint8x8_t nib = vmovn_u16( vandq_u16( vshlq_u16( w, sh ), vdupq_n_u16( 0x0f ) ) );
				bgr = vorrq_u16( vmovl_u8( vtbl2_u8( clut_lo, nib ) ), vshll_n_u8( vtbl2_u8( clut_hi, nib ), 8 ) );
			}
			else if( tp == 1 )
			{
				const int16x8_t sh = vnegq_s16( vreinterpretq_s16_u16( vshlq_n_u16( vandq_u16( n_u, vdupq_n_u16( 1 ) ), 3 ) ) );
				UINT16 a_t[ 8 ];
				vst1q_u16( a_t, vandq_u16( vshlq_u16( w, sh ), vdupq_n_u16( 0xff ) ) );
				for( i = 0; i < 8; i++ )
				{
					a_bgr[ i ] = clut[ a_t[ i ] ];
				}
				bgr = vld1q_u16( a_bgr );
			}
			else
			{
				bgr = w;
			}
			if( m < 8 )
			{
				/* unused lanes: transparent */
				bgr = vandq_u16( bgr, vcltq_u16( vcombine_u16( vmovn_u32( lane_lo ), vmovn_u32( lane_hi ) ), vdupq_n_u16( m ) ) );
			}
			}
			u += (UINT32)du * 8;
			v += (UINT32)dv * 8;
		}
		else
		{
			for( i = 0; i < m; i++ )
			{
				UINT32 n_u = ( u >> 16 ) & umask, n_v = ( v >> 16 ) & vmask;
				if( win )
				{
					n_u = psx_ts_mod( n_u, tww, mw );
					n_v = psx_ts_mod( n_v, twh, mh );
				}
				a_bgr[ i ] = psx_ts_texel( clut, fixrow ? row : m_p_p_vram[ s->ty + n_v ] + s->tx, tp, n_u );
				u += du; v += dv;
			}
			for( ; i < 8; i++ )
			{
				a_bgr[ i ] = 0;
			}
			bgr = vld1q_u16( a_bgr );
		}

		sr = psx_ts_hi16( r_lo, r_hi );
		sg = psx_ts_hi16( g_lo, g_hi );
		sb = psx_ts_hi16( b_lo, b_hi );
		r_lo = vaddq_u32( r_lo, r_step ); r_hi = vaddq_u32( r_hi, r_step );
		g_lo = vaddq_u32( g_lo, g_step ); g_hi = vaddq_u32( g_hi, g_step );
		b_lo = vaddq_u32( b_lo, b_step ); b_hi = vaddq_u32( b_hi, b_step );
		r += (UINT32)s->dr * 8; g += (UINT32)s->dg * 8; b += (UINT32)s->db * 8;

		/* a shade above 255 would mix into the table index: use the tables */
		if( psx_texspan_max( vmaxq_u16( vmaxq_u16( sr, sg ), sb ) ) > 255 )
		{
			vst1q_u16( a_bgr, bgr );
			for( i = 0; i < m; i++ )
			{
				psx_texspan_pixel( s, p_vram + i, a_bgr[ i ], r0, g0, b0 );
				r0 += s->dr; g0 += s->dg; b0 += s->db;
			}
			continue;
		}

		/* level x shade / 128, before the clamp to 31 */
		cr = vandq_u16( bgr, m5 );
		cg = vandq_u16( vshrq_n_u16( bgr, 5 ), m5 );
		cb = vandq_u16( vshrq_n_u16( bgr, 10 ), m5 );
		rr = vshrq_n_u16( vmulq_u16( cr, sr ), 7 );
		rg = vshrq_n_u16( vmulq_u16( cg, sg ), 7 );
		rb = vshrq_n_u16( vmulq_u16( cb, sb ), 7 );

		out = vorrq_u16( vminq_u16( rr, c31 ),
			vorrq_u16( vshlq_n_u16( vminq_u16( rg, c31 ), 5 ), vshlq_n_u16( vminq_u16( rb, c31 ), 10 ) ) );

		dst = vld1q_u16( p_vram );
		if( s->semi )
		{
			uint16x8_t fr, fg, fb, br, bgc, bb, tr, tg, tb, blend;
			br = vandq_u16( dst, m5 );
			bgc = vandq_u16( vshrq_n_u16( dst, 5 ), m5 );
			bb = vshrq_n_u16( dst, 10 );
			bb = vandq_u16( bb, m5 );
			switch( s->abr )
			{
			case 0x00:	/* 0.5 x B + 0.5 x F */
				fr = vminq_u16( vshrq_n_u16( rr, 1 ), c31 ); fg = vminq_u16( vshrq_n_u16( rg, 1 ), c31 ); fb = vminq_u16( vshrq_n_u16( rb, 1 ), c31 );
				br = vshrq_n_u16( br, 1 ); bgc = vshrq_n_u16( bgc, 1 ); bb = vshrq_n_u16( bb, 1 );
				tr = vminq_u16( vaddq_u16( br, fr ), c31 ); tg = vminq_u16( vaddq_u16( bgc, fg ), c31 ); tb = vminq_u16( vaddq_u16( bb, fb ), c31 );
				break;
			case 0x02:	/* 1.0 x B - 1.0 x F */
				fr = vminq_u16( rr, c31 ); fg = vminq_u16( rg, c31 ); fb = vminq_u16( rb, c31 );
				tr = vqsubq_u16( br, fr ); tg = vqsubq_u16( bgc, fg ); tb = vqsubq_u16( bb, fb );
				break;
			case 0x03:	/* 1.0 x B + 0.25 x F */
				fr = vminq_u16( vshrq_n_u16( rr, 2 ), c31 ); fg = vminq_u16( vshrq_n_u16( rg, 2 ), c31 ); fb = vminq_u16( vshrq_n_u16( rb, 2 ), c31 );
				tr = vminq_u16( vaddq_u16( br, fr ), c31 ); tg = vminq_u16( vaddq_u16( bgc, fg ), c31 ); tb = vminq_u16( vaddq_u16( bb, fb ), c31 );
				break;
			default:	/* 1.0 x B + 1.0 x F */
				fr = vminq_u16( rr, c31 ); fg = vminq_u16( rg, c31 ); fb = vminq_u16( rb, c31 );
				tr = vminq_u16( vaddq_u16( br, fr ), c31 ); tg = vminq_u16( vaddq_u16( bgc, fg ), c31 ); tb = vminq_u16( vaddq_u16( bb, fb ), c31 );
				break;
			}
			blend = vorrq_u16( tr, vorrq_u16( vshlq_n_u16( tg, 5 ), vshlq_n_u16( tb, 10 ) ) );
			out = vbslq_u16( vtstq_u16( bgr, bit15 ), blend, out );
		}
		/* colour 0 is transparent (and so are the unused lanes) */
		vst1q_u16( p_vram, vbslq_u16( vceqq_u16( bgr, zero ), dst, out ) );
	}
}

/*
	SOLIDFILL through the same code: an untextured pixel is what the tables
	give for a texel of level 16 on each channel (0x4210), with bit 15 set
	when semi-transparency is on, as every pixel then blends.
*/
static void psx_solidspan_draw( const struct psx_texspan *s, UINT16 *p_vram, int n )
{
	psx_ts_span( s, p_vram, n, 3, 0, 0 );
}

static int psx_texspan_draw( const struct psx_texspan *s, UINT16 *p_vram, int n )
{
	const int win = ( m_n_twh != 256 || m_n_tww != 256 || m_n_twx != 0 || m_n_twy != 0 );

	if( n < 1 || s->tp > 2 || ( win && ( m_n_tww < 2 || m_n_twh < 2 ) ) || psx_texspan_overlap( s, win, p_vram, n ) )
	{
		return 0;
	}

	if( win )
	{
		switch( s->tp )
		{
		case 0: psx_ts_span( s, p_vram, n, 0, 1, 0 ); break;
		case 1: psx_ts_span( s, p_vram, n, 1, 1, 0 ); break;
		default: psx_ts_span( s, p_vram, n, 2, 1, 0 ); break;
		}
	}
	else if( s->dv == 0 )
	{
		switch( s->tp )
		{
		case 0: psx_ts_span( s, p_vram, n, 0, 0, 1 ); break;
		case 1: psx_ts_span( s, p_vram, n, 1, 0, 1 ); break;
		default: psx_ts_span( s, p_vram, n, 2, 0, 1 ); break;
		}
	}
	else
	{
		switch( s->tp )
		{
		case 0: psx_ts_span( s, p_vram, n, 0, 0, 0 ); break;
		case 1: psx_ts_span( s, p_vram, n, 1, 0, 0 ); break;
		default: psx_ts_span( s, p_vram, n, 2, 0, 0 ); break;
		}
	}
	return 1;
}

#endif	/* PSX_TEXSPAN */

#if PSX_TEXSPAN
/*
	TEXTUREFILL for a span of a standard (not interleaved) texture through
	psx_texspan_draw: same right clipping, and the caller's n_distance and
	p_vram end as TEXTUREFILL leaves them. Otherwise nothing is done and
	the caller uses TEXTUREFILL.
*/
#define TEXTURESPAN( T, U, DU, V, DV, UMASK, VMASK, DR, DG, DB ) \
	if( n_ti == 0 && n_tp <= 2 && psx_texspan_fill_##T( n_x, n_y, &n_distance, &p_vram, \
		U, DU, V, DV, UMASK, VMASK, n_r.d, DR, n_g.d, DG, n_b.d, DB, \
		n_tp, n_tx, n_ty, n_abr, ( n_cmd & 0x02 ) != 0, p_clut ) ) \
	{ \
	} \
	else

#ifdef PSX_TEXSPAN_VERIFY
/* debug: the same span through the tables, on a copy, must give the same pixels */
static UINT16 psx_texspan_ref[ 1024 ];
static void psx_texspan_reference( const struct psx_texspan *s, const UINT16 *p_vram, int n )
{
	const int win = ( m_n_twh != 256 || m_n_tww != 256 || m_n_twx != 0 || m_n_twy != 0 );
	UINT32 u = s->u, v = s->v, r = s->r, g = s->g, b = s->b;
	int i;
	memcpy( psx_texspan_ref, p_vram, n * 2 );
	for( i = 0; i < n; i++ )
	{
		psx_texspan_pixel( s, psx_texspan_ref + i, psx_texspan_texel( s, win, u, v ), r, g, b );
		u += s->du; v += s->dv; r += s->dr; g += s->dg; b += s->db;
	}
}
static unsigned long psx_texspan_checked;
static void psx_texspan_compare( const struct psx_texspan *s, const UINT16 *p_vram, int n )
{
	int i;
	psx_texspan_checked++;
	for( i = 0; i < n; i++ )
	{
		if( psx_texspan_ref[ i ] != p_vram[ i ] )
		{
			fprintf( stderr, "TEXSPAN mismatch span #%lu pixel %d/%d: table %04x neon %04x (tp %u semi %d abr %u u %08x du %08x v %08x dv %08x r %08x dr %08x)\n",
				psx_texspan_checked, i, n, psx_texspan_ref[ i ], p_vram[ i ], s->tp, s->semi, s->abr, s->u, s->du, s->v, s->dv, s->r, s->dr );
			exit( 1 );
		}
	}
}
static void psx_texspan_report( void ) __attribute__(( destructor ));
static void psx_texspan_report( void ) { fprintf( stderr, "TEXSPAN checked %lu spans\n", psx_texspan_checked ); }
#define PSX_TEXSPAN_VERIFY_BEFORE	psx_texspan_reference( &s, p_vram, n_distance );
#define PSX_TEXSPAN_VERIFY_AFTER	psx_texspan_compare( &s, p_vram, n_distance );
#else
#define PSX_TEXSPAN_VERIFY_BEFORE
#define PSX_TEXSPAN_VERIFY_AFTER
#endif

#define PSX_TEXSPAN_FILL_DISTANCE( T ) \
static int psx_texspan_fill_##T( INT32 n_x, INT32 n_y, T *p_distance, UINT16 **pp_vram, \
	UINT32 u, INT32 du, UINT32 v, INT32 dv, UINT32 umask, UINT32 vmask, \
	UINT32 r, INT32 dr, UINT32 g, INT32 dg, UINT32 b, INT32 db, \
	UINT32 tp, UINT32 tx, UINT32 ty, UINT32 abr, int semi, const UINT16 *clut ) \
{ \
	struct psx_texspan s; \
	INT32 n_distance = *p_distance; \
	UINT16 *p_vram; \
	if( n_distance > ( (INT32)m_n_drawarea_x2 - n_x ) + 1 ) \
	{ \
		n_distance = ( m_n_drawarea_x2 - n_x ) + 1; \
	} \
	if( n_distance < 1 ) \
	{ \
		return 0; \
	} \
	p_vram = m_p_p_vram[ n_y ] + n_x; \
	s.u = u; s.du = du; s.v = v; s.dv = dv; s.umask = umask; s.vmask = vmask; \
	s.r = r; s.dr = dr; s.g = g; s.dg = dg; s.b = b; s.db = db; \
	s.tp = tp; s.tx = tx; s.ty = ty; s.abr = abr & 3; s.semi = semi; s.clut = clut; \
	PSX_TEXSPAN_VERIFY_BEFORE \
	if( !psx_texspan_draw( &s, p_vram, n_distance ) ) \
	{ \
		return 0; \
	} \
	PSX_TEXSPAN_VERIFY_AFTER \
	*p_distance = 0; \
	*pp_vram = p_vram + n_distance; \
	return 1; \
}
PSX_TEXSPAN_FILL_DISTANCE( INT32 )
PSX_TEXSPAN_FILL_DISTANCE( INT16 )

/* SOLIDFILL for a span: same clipping, n_distance and p_vram end as SOLIDFILL leaves them */
static void psx_solidspan_fill( INT32 n_x, INT32 n_y, INT32 *p_distance, UINT16 **pp_vram,
	UINT32 r, INT32 dr, UINT32 g, INT32 dg, UINT32 b, INT32 db, UINT32 abr, int semi )
{
	struct psx_texspan s;
	INT32 n_distance = *p_distance;
	UINT16 *p_vram;
	if( n_distance > ( (INT32)m_n_drawarea_x2 - n_x ) + 1 )
	{
		n_distance = ( m_n_drawarea_x2 - n_x ) + 1;
	}
	p_vram = m_p_p_vram[ n_y ] + n_x;
	if( n_distance > 0 )
	{
		memset( &s, 0, sizeof( s ) );
		s.r = r; s.dr = dr; s.g = g; s.dg = dg; s.b = b; s.db = db;
		s.tp = 3; s.abr = abr & 3; s.semi = semi;
#ifdef PSX_TEXSPAN_VERIFY
		{
			UINT16 a_ref[ 1024 ];
			UINT32 rr = r, gg = g, bb = b;
			int i;
			memcpy( a_ref, p_vram, n_distance * 2 );
			for( i = 0; i < n_distance; i++ )
			{
				psx_texspan_pixel( &s, a_ref + i, semi ? 0xc210 : 0x4210, rr, gg, bb );
				rr += dr; gg += dg; bb += db;
			}
			psx_solidspan_draw( &s, p_vram, n_distance );
			if( memcmp( a_ref, p_vram, n_distance * 2 ) != 0 )
			{
				fprintf( stderr, "SOLIDSPAN mismatch n %d semi %d abr %u r %08x dr %08x\n", n_distance, semi, abr & 3, r, dr );
				exit( 1 );
			}
		}
#else
		psx_solidspan_draw( &s, p_vram, n_distance );
#endif
		p_vram += n_distance;
		n_distance = 0;
	}
	*p_distance = n_distance;
	*pp_vram = p_vram;
}

#define SOLIDSPAN( DR, DG, DB ) \
	if( 1 ) \
	{ \
		psx_solidspan_fill( n_x, n_y, &n_distance, &p_vram, n_r.d, DR, n_g.d, DG, n_b.d, DB, n_abr, ( n_cmd & 0x02 ) != 0 ); \
	} \
	else
#else
#define TEXTURESPAN( T, U, DU, V, DV, UMASK, VMASK, DR, DG, DB )
#define SOLIDSPAN( DR, DG, DB )
#endif

#define TEXTUREFILL( PIXELUPDATE, TXU, TXV ) \
	if( n_distance > ( (INT32)m_n_drawarea_x2 - n_x ) + 1 ) \
	{ \
		n_distance = ( m_n_drawarea_x2 - n_x ) + 1; \
	} \
	p_vram = m_p_p_vram[ n_y ] + n_x; \
 \
	if( n_ti != 0 ) \
	{ \
		/* interleaved texture */ \
		if( m_n_twh != 256 || \
			m_n_tww != 256 || \
			m_n_twx != 0 || \
			m_n_twy != 0 ) \
		{ \
			/* texture window */ \
			switch( n_cmd & 0x02 ) \
			{ \
			case 0x00: \
				/* shading */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREWINDOWINTERLEAVED4BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREWINDOWINTERLEAVED8BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREWINDOWINTERLEAVED15BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			case 0x02: \
				/* semi transparency */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREWINDOWINTERLEAVED4BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREWINDOWINTERLEAVED8BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREWINDOWINTERLEAVED15BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			} \
		} \
		else \
		{ \
			/* no texture window */ \
			switch( n_cmd & 0x02 ) \
			{ \
			case 0x00: \
				/* shading */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREINTERLEAVED4BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREINTERLEAVED8BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREINTERLEAVED15BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			case 0x02: \
				/* semi transparency */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREINTERLEAVED4BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREINTERLEAVED8BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREINTERLEAVED15BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			} \
		} \
	} \
	else \
	{ \
		/* standard texture */ \
		if( m_n_twh != 256 || \
			m_n_tww != 256 || \
			m_n_twx != 0 || \
			m_n_twy != 0 ) \
		{ \
			/* texture window */ \
			switch( n_cmd & 0x02 ) \
			{ \
			case 0x00: \
				/* shading */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREWINDOW4BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREWINDOW8BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREWINDOW15BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			case 0x02: \
				/* semi transparency */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTUREWINDOW4BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTUREWINDOW8BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTUREWINDOW15BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			} \
		} \
		else \
		{ \
			/* no texture window */ \
			switch( n_cmd & 0x02 ) \
			{ \
			case 0x00: \
				/* shading */ \
				switch( n_tp ) \
				{ \
				case 0: \
					TEXTURE4BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTURE8BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTURE15BIT( TXV, TXU ) \
					SHADEDPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			case 0x02: \
				/* semi transparency */ \
				switch( n_tp ) \
				{ \
				case 0: \
					/* 4 bit clut */ \
					TEXTURE4BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 1: \
					/* 8 bit clut */ \
					TEXTURE8BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				case 2: \
					/* 15 bit */ \
					TEXTURE15BIT( TXV, TXU ) \
					TRANSPARENTPIXEL( PIXELUPDATE ) \
					break; \
				} \
				break; \
			} \
		} \
	}

static void FlatPolygon( int n_points )
{
	INT16 n_y;
	INT16 n_x;

	UINT32 n_abr;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	PAIR n_cx1;
	PAIR n_cx2;
	INT32 n_dx1;
	INT32 n_dx2;

	UINT8 n_cmd;

	INT32 n_distance;

	UINT16 n_point;
	UINT16 n_rightpoint;
	UINT16 n_leftpoint;
	UINT16 *p_n_rightpointlist;
	UINT16 *p_n_leftpointlist;

	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 1 )
	{
		return;
	}
	for( n_point = 0; n_point < n_points; n_point++ )
	{
		DebugMesh( COORD_X( m_packet.FlatPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_y );
	}
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.FlatPolygon.n_bgr );

	SOLIDSETUP( m_n_drawmode )

	n_r.w.h = BGR_R( m_packet.FlatPolygon.n_bgr ); n_r.w.l = 0;
	n_g.w.h = BGR_G( m_packet.FlatPolygon.n_bgr ); n_g.w.l = 0;
	n_b.w.h = BGR_B( m_packet.FlatPolygon.n_bgr ); n_b.w.l = 0;

	if( n_points == 4 )
	{
		p_n_rightpointlist = m_p_n_nextpointlist4;
		p_n_leftpointlist = m_p_n_prevpointlist4;
	}
	else
	{
		p_n_rightpointlist = m_p_n_nextpointlist3;
		p_n_leftpointlist = m_p_n_prevpointlist3;
	}

	for( n_point = 0; n_point < n_points; n_point++ )
	{
		ADJUST_COORD( m_packet.FlatPolygon.vertex[ n_point ].n_coord );
	}

	n_leftpoint = 0;
	for( n_point = 1; n_point < n_points; n_point++ )
	{
		if( COORD_Y( m_packet.FlatPolygon.vertex[ n_point ].n_coord ) < COORD_Y( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) ||
			( COORD_Y( m_packet.FlatPolygon.vertex[ n_point ].n_coord ) == COORD_Y( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) &&
			COORD_X( m_packet.FlatPolygon.vertex[ n_point ].n_coord ) < COORD_X( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) ) )
		{
			n_leftpoint = n_point;
		}
	}
	n_rightpoint = n_leftpoint;

	n_dx1 = 0;
	n_dx2 = 0;

	n_y = COORD_Y( m_packet.FlatPolygon.vertex[ n_rightpoint ].n_coord );

	for( ;; )
	{
		if( n_y == COORD_Y( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.FlatPolygon.vertex[ p_n_leftpointlist[ n_leftpoint ] ].n_coord ) )
			{
				n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
				if( n_leftpoint == n_rightpoint )
				{
					break;
				}
			}
			n_cx1.w.h = COORD_X( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ); n_cx1.w.l = 0;
			n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
			n_distance = COORD_Y( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx1 = (INT32)( ( COORD_X( m_packet.FlatPolygon.vertex[ n_leftpoint ].n_coord ) << 16 ) - n_cx1.d ) / n_distance;
		}
		if( n_y == COORD_Y( m_packet.FlatPolygon.vertex[ n_rightpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.FlatPolygon.vertex[ p_n_rightpointlist[ n_rightpoint ] ].n_coord ) )
			{
				n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
				if( n_rightpoint == n_leftpoint )
				{
					break;
				}
			}
			n_cx2.w.h = COORD_X( m_packet.FlatPolygon.vertex[ n_rightpoint ].n_coord ); n_cx2.w.l = 0;
			n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
			n_distance = COORD_Y( m_packet.FlatPolygon.vertex[ n_rightpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx2 = (INT32)( ( COORD_X( m_packet.FlatPolygon.vertex[ n_rightpoint ].n_coord ) << 16 ) - n_cx2.d ) / n_distance;
		}
		if( (INT16)n_cx1.w.h != (INT16)n_cx2.w.h && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( (INT16)n_cx1.w.h < (INT16)n_cx2.w.h )
			{
				n_x = n_cx1.w.h;
				n_distance = (INT16)n_cx2.w.h - n_x;
			}
			else
			{
				n_x = n_cx2.w.h;
				n_distance = (INT16)n_cx1.w.h - n_x;
			}

			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			SOLIDSPAN( 0, 0, 0 )
			SOLIDFILL( FLATPOLYGONUPDATE )
		}
		n_cx1.d += n_dx1;
		n_cx2.d += n_dx2;
		n_y++;
	}
}

static void FlatTexturedPolygon( int n_points )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;

	UINT32 n_clutx;
	UINT32 n_cluty;

	UINT32 n_tp;
	UINT32 n_tx;
	UINT32 n_ty;
	UINT32 n_abr;
	UINT32 n_ti;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	PAIR n_u;
	PAIR n_v;

	PAIR n_cx1;
	PAIR n_cx2;
	PAIR n_cu1;
	PAIR n_cv1;
	PAIR n_cu2;
	PAIR n_cv2;
	INT32 n_du;
	INT32 n_dv;
	INT32 n_dx1;
	INT32 n_dx2;
	INT32 n_du1;
	INT32 n_dv1;
	INT32 n_du2;
	INT32 n_dv2;

	INT32 n_distance;
	UINT16 n_point;
	UINT16 n_rightpoint;
	UINT16 n_leftpoint;
	UINT16 *p_n_rightpointlist;
	UINT16 *p_n_leftpointlist;
	UINT16 *p_clut;
	UINT16 *p_vram;
	UINT32 n_bgr;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 2 )
	{
		return;
	}
	for( n_point = 0; n_point < n_points; n_point++ )
	{
		DebugMesh( COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_y );
	}
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.FlatTexturedPolygon.n_bgr );

	n_clutx = ( m_packet.FlatTexturedPolygon.vertex[ 0 ].n_texture.w.h & 0x3f ) << 4;
	n_cluty = ( m_packet.FlatTexturedPolygon.vertex[ 0 ].n_texture.w.h >> 6 ) & 0x3ff;

	TEXTURESETUP( m_packet.FlatTexturedPolygon.vertex[ 1 ].n_texture.w.h )

	switch( n_cmd & 0x01 )
	{
	case 0:
		n_r.w.h = BGR_R( m_packet.FlatTexturedPolygon.n_bgr ); n_r.w.l = 0;
		n_g.w.h = BGR_G( m_packet.FlatTexturedPolygon.n_bgr ); n_g.w.l = 0;
		n_b.w.h = BGR_B( m_packet.FlatTexturedPolygon.n_bgr ); n_b.w.l = 0;
		break;
	case 1:
		n_r.w.h = 0x80; n_r.w.l = 0;
		n_g.w.h = 0x80; n_g.w.l = 0;
		n_b.w.h = 0x80; n_b.w.l = 0;
		break;
	}

	if( n_points == 4 )
	{
		p_n_rightpointlist = m_p_n_nextpointlist4;
		p_n_leftpointlist = m_p_n_prevpointlist4;
	}
	else
	{
		p_n_rightpointlist = m_p_n_nextpointlist3;
		p_n_leftpointlist = m_p_n_prevpointlist3;
	}

	for( n_point = 0; n_point < n_points; n_point++ )
	{
		ADJUST_COORD( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord );
	}

	n_leftpoint = 0;
	for( n_point = 1; n_point < n_points; n_point++ )
	{
		if( COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord ) < COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) ||
			( COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord ) == COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) &&
			COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_point ].n_coord ) < COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) ) )
		{
			n_leftpoint = n_point;
		}
	}
	n_rightpoint = n_leftpoint;

	n_dx1 = 0;
	n_dx2 = 0;
	n_du1 = 0;
	n_du2 = 0;
	n_dv1 = 0;
	n_dv2 = 0;

	n_y = COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_coord );

	for( ;; )
	{
		if( n_y == COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.FlatTexturedPolygon.vertex[ p_n_leftpointlist[ n_leftpoint ] ].n_coord ) )
			{
				n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
				if( n_leftpoint == n_rightpoint )
				{
					break;
				}
			}
			n_cx1.w.h = COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ); n_cx1.w.l = 0;
			n_cu1.w.h = TEXTURE_U( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_texture ); n_cu1.w.l = 0;
			n_cv1.w.h = TEXTURE_V( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_texture ); n_cv1.w.l = 0;
			n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
			n_distance = COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx1 = (INT32)( ( COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_coord ) << 16 ) - n_cx1.d ) / n_distance;
			n_du1 = (INT32)( ( TEXTURE_U( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_texture ) << 16 ) - n_cu1.d ) / n_distance;
			n_dv1 = (INT32)( ( TEXTURE_V( m_packet.FlatTexturedPolygon.vertex[ n_leftpoint ].n_texture ) << 16 ) - n_cv1.d ) / n_distance;
		}
		if( n_y == COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.FlatTexturedPolygon.vertex[ p_n_rightpointlist[ n_rightpoint ] ].n_coord ) )
			{
				n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
				if( n_rightpoint == n_leftpoint )
				{
					break;
				}
			}
			n_cx2.w.h = COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_coord ); n_cx2.w.l = 0;
			n_cu2.w.h = TEXTURE_U( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_texture ); n_cu2.w.l = 0;
			n_cv2.w.h = TEXTURE_V( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_texture ); n_cv2.w.l = 0;
			n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
			n_distance = COORD_Y( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx2 = (INT32)( ( COORD_X( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_coord ) << 16 ) - n_cx2.d ) / n_distance;
			n_du2 = (INT32)( ( TEXTURE_U( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_texture ) << 16 ) - n_cu2.d ) / n_distance;
			n_dv2 = (INT32)( ( TEXTURE_V( m_packet.FlatTexturedPolygon.vertex[ n_rightpoint ].n_texture ) << 16 ) - n_cv2.d ) / n_distance;
		}
		if( (INT16)n_cx1.w.h != (INT16)n_cx2.w.h && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( (INT16)n_cx1.w.h < (INT16)n_cx2.w.h )
			{
				n_x = n_cx1.w.h;
				n_distance = (INT16)n_cx2.w.h - n_x;

				n_u.d = n_cu1.d;
				n_v.d = n_cv1.d;
				n_du = (INT32)( n_cu2.d - n_cu1.d ) / n_distance;
				n_dv = (INT32)( n_cv2.d - n_cv1.d ) / n_distance;
			}
			else
			{
				n_x = n_cx2.w.h;
				n_distance = (INT16)n_cx1.w.h - n_x;

				n_u.d = n_cu2.d;
				n_v.d = n_cv2.d;
				n_du = (INT32)( n_cu1.d - n_cu2.d ) / n_distance;
				n_dv = (INT32)( n_cv1.d - n_cv2.d ) / n_distance;
			}

			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_u.d += n_du * ( m_n_drawarea_x1 - n_x );
				n_v.d += n_dv * ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			TEXTURESPAN( INT32, n_u.d, n_du, n_v.d, n_dv, 0xffff, 0xffff, 0, 0, 0 )
			{
				TEXTUREFILL( FLATTEXTUREDPOLYGONUPDATE, n_u.w.h, n_v.w.h );
			}
		}
		n_cx1.d += n_dx1;
		n_cu1.d += n_du1;
		n_cv1.d += n_dv1;
		n_cx2.d += n_dx2;
		n_cu2.d += n_du2;
		n_cv2.d += n_dv2;
		n_y++;
	}
}

static void GouraudPolygon( int n_points )
{
	INT16 n_y;
	INT16 n_x;

	UINT32 n_abr;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	UINT8 n_cmd;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	PAIR n_cx1;
	PAIR n_cx2;
	PAIR n_cr1;
	PAIR n_cg1;
	PAIR n_cb1;
	PAIR n_cr2;
	PAIR n_cg2;
	PAIR n_cb2;
	INT32 n_dr;
	INT32 n_dg;
	INT32 n_db;
	INT32 n_dx1;
	INT32 n_dx2;
	INT32 n_dr1;
	INT32 n_dg1;
	INT32 n_db1;
	INT32 n_dr2;
	INT32 n_dg2;
	INT32 n_db2;

	INT32 n_distance;

	UINT16 n_point;
	UINT16 n_rightpoint;
	UINT16 n_leftpoint;
	UINT16 *p_n_rightpointlist;
	UINT16 *p_n_leftpointlist;

	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 3 )
	{
		return;
	}
	for( n_point = 0; n_point < n_points; n_point++ )
	{
		DebugMesh( COORD_X( m_packet.GouraudPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.GouraudPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_y );
	}
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.GouraudPolygon.vertex[ 0 ].n_bgr );

	SOLIDSETUP( m_n_drawmode )

	if( n_points == 4 )
	{
		p_n_rightpointlist = m_p_n_nextpointlist4;
		p_n_leftpointlist = m_p_n_prevpointlist4;
	}
	else
	{
		p_n_rightpointlist = m_p_n_nextpointlist3;
		p_n_leftpointlist = m_p_n_prevpointlist3;
	}

	for( n_point = 0; n_point < n_points; n_point++ )
	{
		ADJUST_COORD( m_packet.GouraudPolygon.vertex[ n_point ].n_coord );
	}

	n_leftpoint = 0;
	for( n_point = 1; n_point < n_points; n_point++ )
	{
		if( COORD_Y( m_packet.GouraudPolygon.vertex[ n_point ].n_coord ) < COORD_Y( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) ||
			( COORD_Y( m_packet.GouraudPolygon.vertex[ n_point ].n_coord ) == COORD_Y( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) &&
			COORD_X( m_packet.GouraudPolygon.vertex[ n_point ].n_coord ) < COORD_X( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) ) )
		{
			n_leftpoint = n_point;
		}
	}
	n_rightpoint = n_leftpoint;

	n_dx1 = 0;
	n_dx2 = 0;
	n_dr1 = 0;
	n_dr2 = 0;
	n_dg1 = 0;
	n_dg2 = 0;
	n_db1 = 0;
	n_db2 = 0;

	n_y = COORD_Y( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_coord );

	for( ;; )
	{
		if( n_y == COORD_Y( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.GouraudPolygon.vertex[ p_n_leftpointlist[ n_leftpoint ] ].n_coord ) )
			{
				n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
				if( n_leftpoint == n_rightpoint )
				{
					break;
				}
			}
			n_cx1.w.h = COORD_X( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ); n_cx1.w.l = 0;
			n_cr1.w.h = BGR_R( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ); n_cr1.w.l = 0;
			n_cg1.w.h = BGR_G( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ); n_cg1.w.l = 0;
			n_cb1.w.h = BGR_B( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ); n_cb1.w.l = 0;
			n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
			n_distance = COORD_Y( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx1 = (INT32)( ( COORD_X( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_coord ) << 16 ) - n_cx1.d ) / n_distance;
			n_dr1 = (INT32)( ( BGR_R( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cr1.d ) / n_distance;
			n_dg1 = (INT32)( ( BGR_G( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cg1.d ) / n_distance;
			n_db1 = (INT32)( ( BGR_B( m_packet.GouraudPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cb1.d ) / n_distance;
		}
		if( n_y == COORD_Y( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.GouraudPolygon.vertex[ p_n_rightpointlist[ n_rightpoint ] ].n_coord ) )
			{
				n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
				if( n_rightpoint == n_leftpoint )
				{
					break;
				}
			}
			n_cx2.w.h = COORD_X( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_coord ); n_cx2.w.l = 0;
			n_cr2.w.h = BGR_R( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ); n_cr2.w.l = 0;
			n_cg2.w.h = BGR_G( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ); n_cg2.w.l = 0;
			n_cb2.w.h = BGR_B( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ); n_cb2.w.l = 0;
			n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
			n_distance = COORD_Y( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx2 = (INT32)( ( COORD_X( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_coord ) << 16 ) - n_cx2.d ) / n_distance;
			n_dr2 = (INT32)( ( BGR_R( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cr2.d ) / n_distance;
			n_dg2 = (INT32)( ( BGR_G( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cg2.d ) / n_distance;
			n_db2 = (INT32)( ( BGR_B( m_packet.GouraudPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cb2.d ) / n_distance;
		}
		if( (INT16)n_cx1.w.h != (INT16)n_cx2.w.h && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( (INT16)n_cx1.w.h < (INT16)n_cx2.w.h )
			{
				n_x = n_cx1.w.h;
				n_distance = (INT16)n_cx2.w.h - n_x;

				n_r.d = n_cr1.d;
				n_g.d = n_cg1.d;
				n_b.d = n_cb1.d;
				n_dr = (INT32)( n_cr2.d - n_cr1.d ) / n_distance;
				n_dg = (INT32)( n_cg2.d - n_cg1.d ) / n_distance;
				n_db = (INT32)( n_cb2.d - n_cb1.d ) / n_distance;
			}
			else
			{
				n_x = n_cx2.w.h;
				n_distance = (INT16)n_cx1.w.h - n_x;

				n_r.d = n_cr2.d;
				n_g.d = n_cg2.d;
				n_b.d = n_cb2.d;
				n_dr = (INT32)( n_cr1.d - n_cr2.d ) / n_distance;
				n_dg = (INT32)( n_cg1.d - n_cg2.d ) / n_distance;
				n_db = (INT32)( n_cb1.d - n_cb2.d ) / n_distance;
			}

			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_r.d += n_dr * ( m_n_drawarea_x1 - n_x );
				n_g.d += n_dg * ( m_n_drawarea_x1 - n_x );
				n_b.d += n_db * ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			SOLIDSPAN( n_dr, n_dg, n_db )
			SOLIDFILL( GOURAUDPOLYGONUPDATE )
		}
		n_cx1.d += n_dx1;
		n_cr1.d += n_dr1;
		n_cg1.d += n_dg1;
		n_cb1.d += n_db1;
		n_cx2.d += n_dx2;
		n_cr2.d += n_dr2;
		n_cg2.d += n_dg2;
		n_cb2.d += n_db2;
		n_y++;
	}
}

static void GouraudTexturedPolygon( int n_points )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;

	UINT32 n_clutx;
	UINT32 n_cluty;

	UINT32 n_tp;
	UINT32 n_tx;
	UINT32 n_ty;
	UINT32 n_abr;
	UINT32 n_ti;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	PAIR n_u;
	PAIR n_v;

	PAIR n_cx1;
	PAIR n_cx2;
	PAIR n_cu1;
	PAIR n_cv1;
	PAIR n_cu2;
	PAIR n_cv2;
	PAIR n_cr1;
	PAIR n_cg1;
	PAIR n_cb1;
	PAIR n_cr2;
	PAIR n_cg2;
	PAIR n_cb2;
	INT32 n_dr;
	INT32 n_dg;
	INT32 n_db;
	INT32 n_du;
	INT32 n_dv;
	INT32 n_dx1;
	INT32 n_dx2;
	INT32 n_dr1;
	INT32 n_dg1;
	INT32 n_db1;
	INT32 n_dr2;
	INT32 n_dg2;
	INT32 n_db2;
	INT32 n_du1;
	INT32 n_dv1;
	INT32 n_du2;
	INT32 n_dv2;

	INT32 n_distance;
	UINT16 n_point;
	UINT16 n_rightpoint;
	UINT16 n_leftpoint;
	UINT16 *p_n_rightpointlist;
	UINT16 *p_n_leftpointlist;
	UINT16 *p_clut;
	UINT16 *p_vram;
	UINT32 n_bgr;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 4 )
	{
		return;
	}
	for( n_point = 0; n_point < n_points; n_point++ )
	{
		DebugMesh( COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord ) + m_n_drawoffset_y );
	}
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.GouraudTexturedPolygon.vertex[ 0 ].n_bgr );

	n_clutx = ( m_packet.GouraudTexturedPolygon.vertex[ 0 ].n_texture.w.h & 0x3f ) << 4;
	n_cluty = ( m_packet.GouraudTexturedPolygon.vertex[ 0 ].n_texture.w.h >> 6 ) & 0x3ff;

	TEXTURESETUP( m_packet.GouraudTexturedPolygon.vertex[ 1 ].n_texture.w.h )

	if( n_points == 4 )
	{
		p_n_rightpointlist = m_p_n_nextpointlist4;
		p_n_leftpointlist = m_p_n_prevpointlist4;
	}
	else
	{
		p_n_rightpointlist = m_p_n_nextpointlist3;
		p_n_leftpointlist = m_p_n_prevpointlist3;
	}

	for( n_point = 0; n_point < n_points; n_point++ )
	{
		ADJUST_COORD( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord );
	}

	n_leftpoint = 0;
	for( n_point = 1; n_point < n_points; n_point++ )
	{
		if( COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord ) < COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) ||
			( COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord ) == COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) &&
			COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_point ].n_coord ) < COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) ) )
		{
			n_leftpoint = n_point;
		}
	}
	n_rightpoint = n_leftpoint;

	n_dx1 = 0;
	n_dx2 = 0;
	n_du1 = 0;
	n_du2 = 0;
	n_dr1 = 0;
	n_dr2 = 0;
	n_dg1 = 0;
	n_dg2 = 0;
	n_db1 = 0;
	n_db2 = 0;
	n_dv1 = 0;
	n_dv2 = 0;

	n_y = COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_coord );

	for( ;; )
	{
		if( n_y == COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ p_n_leftpointlist[ n_leftpoint ] ].n_coord ) )
			{
				n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
				if( n_leftpoint == n_rightpoint )
				{
					break;
				}
			}
			n_cx1.w.h = COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ); n_cx1.w.l = 0;
			switch( n_cmd & 0x01 )
			{
			case 0x00:
				n_cr1.w.h = BGR_R( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ); n_cr1.w.l = 0;
				n_cg1.w.h = BGR_G( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ); n_cg1.w.l = 0;
				n_cb1.w.h = BGR_B( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ); n_cb1.w.l = 0;
				break;
			case 0x01:
				n_cr1.w.h = 0x80; n_cr1.w.l = 0;
				n_cg1.w.h = 0x80; n_cg1.w.l = 0;
				n_cb1.w.h = 0x80; n_cb1.w.l = 0;
				break;
			}
			n_cu1.w.h = TEXTURE_U( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_texture ); n_cu1.w.l = 0;
			n_cv1.w.h = TEXTURE_V( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_texture ); n_cv1.w.l = 0;
			n_leftpoint = p_n_leftpointlist[ n_leftpoint ];
			n_distance = COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx1 = (INT32)( ( COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_coord ) << 16 ) - n_cx1.d ) / n_distance;
			switch( n_cmd & 0x01 )
			{
			case 0x00:
				n_dr1 = (INT32)( ( BGR_R( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cr1.d ) / n_distance;
				n_dg1 = (INT32)( ( BGR_G( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cg1.d ) / n_distance;
				n_db1 = (INT32)( ( BGR_B( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_bgr ) << 16 ) - n_cb1.d ) / n_distance;
				break;
			case 0x01:
				n_dr1 = 0;
				n_dg1 = 0;
				n_db1 = 0;
				break;
			}
			n_du1 = (INT32)( ( TEXTURE_U( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_texture ) << 16 ) - n_cu1.d ) / n_distance;
			n_dv1 = (INT32)( ( TEXTURE_V( m_packet.GouraudTexturedPolygon.vertex[ n_leftpoint ].n_texture ) << 16 ) - n_cv1.d ) / n_distance;
		}
		if( n_y == COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_coord ) )
		{
			while( n_y == COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ p_n_rightpointlist[ n_rightpoint ] ].n_coord ) )
			{
				n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
				if( n_rightpoint == n_leftpoint )
				{
					break;
				}
			}
			n_cx2.w.h = COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_coord ); n_cx2.w.l = 0;
			switch( n_cmd & 0x01 )
			{
			case 0x00:
				n_cr2.w.h = BGR_R( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ); n_cr2.w.l = 0;
				n_cg2.w.h = BGR_G( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ); n_cg2.w.l = 0;
				n_cb2.w.h = BGR_B( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ); n_cb2.w.l = 0;
				break;
			case 0x01:
				n_cr2.w.h = 0x80; n_cr2.w.l = 0;
				n_cg2.w.h = 0x80; n_cg2.w.l = 0;
				n_cb2.w.h = 0x80; n_cb2.w.l = 0;
				break;
			}
			n_cu2.w.h = TEXTURE_U( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_texture ); n_cu2.w.l = 0;
			n_cv2.w.h = TEXTURE_V( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_texture ); n_cv2.w.l = 0;
			n_rightpoint = p_n_rightpointlist[ n_rightpoint ];
			n_distance = COORD_Y( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_coord ) - n_y;
			if( n_distance < 1 )
			{
				break;
			}
			n_dx2 = (INT32)( ( COORD_X( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_coord ) << 16 ) - n_cx2.d ) / n_distance;
			switch( n_cmd & 0x01 )
			{
			case 0x00:
				n_dr2 = (INT32)( ( BGR_R( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cr2.d ) / n_distance;
				n_dg2 = (INT32)( ( BGR_G( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cg2.d ) / n_distance;
				n_db2 = (INT32)( ( BGR_B( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_bgr ) << 16 ) - n_cb2.d ) / n_distance;
				break;
			case 0x01:
				n_dr2 = 0;
				n_dg2 = 0;
				n_db2 = 0;
				break;
			}
			n_du2 = (INT32)( ( TEXTURE_U( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_texture ) << 16 ) - n_cu2.d ) / n_distance;
			n_dv2 = (INT32)( ( TEXTURE_V( m_packet.GouraudTexturedPolygon.vertex[ n_rightpoint ].n_texture ) << 16 ) - n_cv2.d ) / n_distance;
		}
		if( (INT16)n_cx1.w.h != (INT16)n_cx2.w.h && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( (INT16)n_cx1.w.h < (INT16)n_cx2.w.h )
			{
				n_x = n_cx1.w.h;
				n_distance = (INT16)n_cx2.w.h - n_x;

				n_r.d = n_cr1.d;
				n_g.d = n_cg1.d;
				n_b.d = n_cb1.d;
				n_u.d = n_cu1.d;
				n_v.d = n_cv1.d;
				n_dr = (INT32)( n_cr2.d - n_cr1.d ) / n_distance;
				n_dg = (INT32)( n_cg2.d - n_cg1.d ) / n_distance;
				n_db = (INT32)( n_cb2.d - n_cb1.d ) / n_distance;
				n_du = (INT32)( n_cu2.d - n_cu1.d ) / n_distance;
				n_dv = (INT32)( n_cv2.d - n_cv1.d ) / n_distance;
			}
			else
			{
				n_x = n_cx2.w.h;
				n_distance = (INT16)n_cx1.w.h - n_x;

				n_r.d = n_cr2.d;
				n_g.d = n_cg2.d;
				n_b.d = n_cb2.d;
				n_u.d = n_cu2.d;
				n_v.d = n_cv2.d;
				n_dr = (INT32)( n_cr1.d - n_cr2.d ) / n_distance;
				n_dg = (INT32)( n_cg1.d - n_cg2.d ) / n_distance;
				n_db = (INT32)( n_cb1.d - n_cb2.d ) / n_distance;
				n_du = (INT32)( n_cu1.d - n_cu2.d ) / n_distance;
				n_dv = (INT32)( n_cv1.d - n_cv2.d ) / n_distance;
			}

			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_r.d += n_dr * ( m_n_drawarea_x1 - n_x );
				n_g.d += n_dg * ( m_n_drawarea_x1 - n_x );
				n_b.d += n_db * ( m_n_drawarea_x1 - n_x );
				n_u.d += n_du * ( m_n_drawarea_x1 - n_x );
				n_v.d += n_dv * ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			TEXTURESPAN( INT32, n_u.d, n_du, n_v.d, n_dv, 0xffff, 0xffff, n_dr, n_dg, n_db )
			{
				TEXTUREFILL( GOURAUDTEXTUREDPOLYGONUPDATE, n_u.w.h, n_v.w.h );
			}
		}
		n_cx1.d += n_dx1;
		n_cr1.d += n_dr1;
		n_cg1.d += n_dg1;
		n_cb1.d += n_db1;
		n_cu1.d += n_du1;
		n_cv1.d += n_dv1;
		n_cx2.d += n_dx2;
		n_cr2.d += n_dr2;
		n_cg2.d += n_dg2;
		n_cb2.d += n_db2;
		n_cu2.d += n_du2;
		n_cv2.d += n_dv2;
		n_y++;
	}
}

static void MonochromeLine( void )
{
	PAIR n_x;
	PAIR n_y;
	INT32 n_dx;
	INT32 n_dy;
	INT32 n_dr;
	INT32 n_dg;
	INT32 n_db;
	INT32 n_len;
	INT32 n_xlen;
	INT32 n_ylen;
	INT32 n_xstart;
	INT32 n_ystart;
	INT32 n_xend;
	INT32 n_yend;
	UINT32 n_r;
	UINT32 n_g;
	UINT32 n_b;
	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 5 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.MonochromeLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.MonochromeLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.MonochromeLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.MonochromeLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_y );
	DebugMeshEnd();
#endif

	n_xstart = COORD_X( m_packet.MonochromeLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_x;
	n_xend = COORD_X( m_packet.MonochromeLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_x;
	n_ystart = COORD_Y( m_packet.MonochromeLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_y;
	n_yend = COORD_Y( m_packet.MonochromeLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_y;

	n_r = BGR_R( m_packet.MonochromeLine.n_bgr );
	n_g = BGR_G( m_packet.MonochromeLine.n_bgr );
	n_b = BGR_B( m_packet.MonochromeLine.n_bgr );

	if( n_xend > n_xstart )
	{
		n_xlen = n_xend - n_xstart;
	}
	else
	{
		n_xlen = n_xstart - n_xend;
	}

	if( n_yend > n_ystart )
	{
		n_ylen = n_yend - n_ystart;
	}
	else
	{
		n_ylen = n_ystart - n_yend;
	}

	if( n_xlen > n_ylen )
	{
		n_len = n_xlen;
	}
	else
	{
		n_len = n_ylen;
	}

	if( n_len == 0 )
	{
		n_len = 1;
	}

	n_x.w.h = n_xstart; n_x.w.l = 0;
	n_y.w.h = n_ystart; n_y.w.l = 0;

	n_dx = (INT32)( ( n_xend << 16 ) - n_x.d ) / n_len;
	n_dy = (INT32)( ( n_yend << 16 ) - n_y.d ) / n_len;
	n_dr = 0;
	n_dg = 0;
	n_db = 0;

	while( n_len > 0 )
	{
		if( (INT16)n_x.w.h >= (INT32)m_n_drawarea_x1 &&
			(INT16)n_y.w.h >= (INT32)m_n_drawarea_y1 &&
			(INT16)n_x.w.h <= (INT32)m_n_drawarea_x2 &&
			(INT16)n_y.w.h <= (INT32)m_n_drawarea_y2 )
		{
			p_vram = m_p_p_vram[ n_y.w.h ] + n_x.w.h;
			*( p_vram ) = m_p_n_redshade[ MID_LEVEL | n_r ] |
				m_p_n_greenshade[ MID_LEVEL | n_g ] |
				m_p_n_blueshade[ MID_LEVEL | n_b ];
		}
		n_x.d += n_dx;
		n_y.d += n_dy;
		n_len--;
	}
}

static void GouraudLine( void )
{
	PAIR n_x;
	PAIR n_y;
	INT32 n_dx;
	INT32 n_dy;
	INT32 n_dr;
	INT32 n_dg;
	INT32 n_db;
	INT32 n_distance;
	INT32 n_xlen;
	INT32 n_ylen;
	INT32 n_xstart;
	INT32 n_ystart;
	INT32 n_xend;
	INT32 n_yend;
	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	PAIR n_cr1;
	PAIR n_cg1;
	PAIR n_cb1;
	PAIR n_cr2;
	PAIR n_cg2;
	PAIR n_cb2;
	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 6 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.GouraudLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.GouraudLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.GouraudLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.GouraudLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_y );
	DebugMeshEnd();
#endif

	n_xstart = COORD_X( m_packet.GouraudLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_x;
	n_ystart = COORD_Y( m_packet.GouraudLine.vertex[ 0 ].n_coord ) + m_n_drawoffset_y;
	n_cr1.w.h = BGR_R( m_packet.GouraudLine.vertex[ 0 ].n_bgr ); n_cr1.w.l = 0;
	n_cg1.w.h = BGR_G( m_packet.GouraudLine.vertex[ 0 ].n_bgr ); n_cg1.w.l = 0;
	n_cb1.w.h = BGR_B( m_packet.GouraudLine.vertex[ 0 ].n_bgr ); n_cb1.w.l = 0;

	n_xend = COORD_X( m_packet.GouraudLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_x;
	n_yend = COORD_Y( m_packet.GouraudLine.vertex[ 1 ].n_coord ) + m_n_drawoffset_y;
	n_cr2.w.h = BGR_R( m_packet.GouraudLine.vertex[ 1 ].n_bgr ); n_cr1.w.l = 0;
	n_cg2.w.h = BGR_G( m_packet.GouraudLine.vertex[ 1 ].n_bgr ); n_cg1.w.l = 0;
	n_cb2.w.h = BGR_B( m_packet.GouraudLine.vertex[ 1 ].n_bgr ); n_cb1.w.l = 0;

	n_x.w.h = n_xstart; n_x.w.l = 0;
	n_y.w.h = n_ystart; n_y.w.l = 0;
	n_r.d = n_cr1.d;
	n_g.d = n_cg1.d;
	n_b.d = n_cb1.d;

	if( n_xend > n_xstart )
	{
		n_xlen = n_xend - n_xstart;
	}
	else
	{
		n_xlen = n_xstart - n_xend;
	}

	if( n_yend > n_ystart )
	{
		n_ylen = n_yend - n_ystart;
	}
	else
	{
		n_ylen = n_ystart - n_yend;
	}

	if( n_xlen > n_ylen )
	{
		n_distance = n_xlen;
	}
	else
	{
		n_distance = n_ylen;
	}

	if( n_distance == 0 )
	{
		n_distance = 1;
	}

	n_dx = (INT32)( ( n_xend << 16 ) - n_x.d ) / n_distance;
	n_dy = (INT32)( ( n_yend << 16 ) - n_y.d ) / n_distance;
	n_dr = (INT32)( n_cr2.d - n_cr1.d ) / n_distance;
	n_dg = (INT32)( n_cg2.d - n_cg1.d ) / n_distance;
	n_db = (INT32)( n_cb2.d - n_cb1.d ) / n_distance;

	while( n_distance > 0 )
	{
		if( (INT16)n_x.w.h >= (INT32)m_n_drawarea_x1 &&
			(INT16)n_y.w.h >= (INT32)m_n_drawarea_y1 &&
			(INT16)n_x.w.h <= (INT32)m_n_drawarea_x2 &&
			(INT16)n_y.w.h <= (INT32)m_n_drawarea_y2 )
		{
			p_vram = m_p_p_vram[ n_y.w.h ] + n_x.w.h;
			*( p_vram ) = m_p_n_redshade[ MID_LEVEL | n_r.w.h ] |
				m_p_n_greenshade[ MID_LEVEL | n_g.w.h ] |
				m_p_n_blueshade[ MID_LEVEL | n_b.w.h ];
		}
		n_x.d += n_dx;
		n_y.d += n_dy;
		n_r.d += n_dr;
		n_g.d += n_dg;
		n_b.d += n_db;
		n_distance--;
	}
}

static void FrameBufferRectangleDraw( void )
{
	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	INT32 n_distance;
	INT32 n_h;
	INT16 n_y;
	INT16 n_x;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 7 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ), COORD_Y( m_packet.FlatRectangle.n_coord ) );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + SIZE_W( m_packet.FlatRectangle.n_size ), COORD_Y( m_packet.FlatRectangle.n_coord ) );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ), COORD_Y( m_packet.FlatRectangle.n_coord ) + SIZE_H( m_packet.FlatRectangle.n_size ) );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + SIZE_W( m_packet.FlatRectangle.n_size ), COORD_Y( m_packet.FlatRectangle.n_coord ) + SIZE_H( m_packet.FlatRectangle.n_size ) );
	DebugMeshEnd();
#endif

	n_r.w.h = BGR_R( m_packet.FlatRectangle.n_bgr ); n_r.w.l = 0;
	n_g.w.h = BGR_G( m_packet.FlatRectangle.n_bgr ); n_g.w.l = 0;
	n_b.w.h = BGR_B( m_packet.FlatRectangle.n_bgr ); n_b.w.l = 0;

	n_y = COORD_Y( m_packet.FlatRectangle.n_coord );
	n_h = SIZE_H( m_packet.FlatRectangle.n_size );

	while( n_h > 0 )
	{
		n_x = COORD_X( m_packet.FlatRectangle.n_coord );

		n_distance = SIZE_W( m_packet.FlatRectangle.n_size );
		while( n_distance > 0 )
		{
			*( m_p_p_vram[ n_y & 1023 ] + ( n_x & 1023 ) ) =
				m_p_n_redshade[ MID_LEVEL | n_r.w.h ] |
				m_p_n_greenshade[ MID_LEVEL | n_g.w.h ] |
				m_p_n_blueshade[ MID_LEVEL | n_b.w.h ];
			n_x++;
			n_distance--;
		}
		n_y++;
		n_h--;
	}
}

static void FlatRectangle( void )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;
	UINT32 n_abr;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;

	INT32 n_distance;
	INT32 n_h;
	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 8 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_x + SIZE_W( m_packet.FlatRectangle.n_size ), COORD_Y( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_y + SIZE_H( m_packet.FlatRectangle.n_size ) );
	DebugMesh( COORD_X( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_x + SIZE_W( m_packet.FlatRectangle.n_size ), COORD_Y( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_y + SIZE_H( m_packet.FlatRectangle.n_size ) );
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.FlatRectangle.n_bgr );

	SOLIDSETUP( m_n_drawmode )

	n_r.w.h = BGR_R( m_packet.FlatRectangle.n_bgr ); n_r.w.l = 0;
	n_g.w.h = BGR_G( m_packet.FlatRectangle.n_bgr ); n_g.w.l = 0;
	n_b.w.h = BGR_B( m_packet.FlatRectangle.n_bgr ); n_b.w.l = 0;

	n_y = COORD_Y( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_y;
	n_h = SIZE_H( m_packet.FlatRectangle.n_size );

	while( n_h > 0 )
	{
		n_x = COORD_X( m_packet.FlatRectangle.n_coord ) + m_n_drawoffset_x;

		n_distance = SIZE_W( m_packet.FlatRectangle.n_size );
		if( n_distance > 0 && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			SOLIDSPAN( 0, 0, 0 )
			SOLIDFILL( FLATRECTANGEUPDATE )
		}
		n_y++;
		n_h--;
	}
}

static void FlatTexturedRectangle( void )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;

	UINT32 n_clutx;
	UINT32 n_cluty;

	UINT32 n_tp;
	UINT32 n_tx;
	UINT32 n_ty;
	UINT32 n_abr;
	UINT32 n_ti;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	UINT8 n_u;
	UINT8 n_v;

	INT16 n_distance;
	UINT32 n_h;
	UINT16 *p_vram;
	UINT16 *p_clut;
	UINT16 n_bgr;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 9 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_x + SIZE_W( m_packet.FlatTexturedRectangle.n_size ), COORD_Y( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_y + SIZE_H( m_packet.FlatTexturedRectangle.n_size ) );
	DebugMesh( COORD_X( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_x + SIZE_W( m_packet.FlatTexturedRectangle.n_size ), COORD_Y( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_y + SIZE_H( m_packet.FlatTexturedRectangle.n_size ) );
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.FlatTexturedRectangle.n_bgr );

	n_clutx = ( m_packet.FlatTexturedRectangle.n_texture.w.h & 0x3f ) << 4;
	n_cluty = ( m_packet.FlatTexturedRectangle.n_texture.w.h >> 6 ) & 0x3ff;

	TEXTURESETUP( m_n_drawmode )

	switch( n_cmd & 0x01 )
	{
	case 0:
		n_r.w.h = BGR_R( m_packet.FlatTexturedRectangle.n_bgr ); n_r.w.l = 0;
		n_g.w.h = BGR_G( m_packet.FlatTexturedRectangle.n_bgr ); n_g.w.l = 0;
		n_b.w.h = BGR_B( m_packet.FlatTexturedRectangle.n_bgr ); n_b.w.l = 0;
		break;
	case 1:
		n_r.w.h = 0x80; n_r.w.l = 0;
		n_g.w.h = 0x80; n_g.w.l = 0;
		n_b.w.h = 0x80; n_b.w.l = 0;
		break;
	}

	n_v = TEXTURE_V( m_packet.FlatTexturedRectangle.n_texture );
	n_y = COORD_Y( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_y;
	n_h = SIZE_H( m_packet.FlatTexturedRectangle.n_size );

	while( n_h > 0 )
	{
		n_x = COORD_X( m_packet.FlatTexturedRectangle.n_coord ) + m_n_drawoffset_x;
		n_u = TEXTURE_U( m_packet.FlatTexturedRectangle.n_texture );

		n_distance = SIZE_W( m_packet.FlatTexturedRectangle.n_size );
		if( n_distance > 0 && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_u += ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			TEXTURESPAN( INT16, (UINT32)n_u << 16, 1 << 16, (UINT32)n_v << 16, 0, 0xff, 0xff, 0, 0, 0 )
			{
				TEXTUREFILL( FLATTEXTUREDRECTANGLEUPDATE, n_u, n_v );
			}
		}
		n_v++;
		n_y++;
		n_h--;
	}
}

static void Sprite8x8( void )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;

	UINT32 n_clutx;
	UINT32 n_cluty;

	UINT32 n_tp;
	UINT32 n_tx;
	UINT32 n_ty;
	UINT32 n_abr;
	UINT32 n_ti;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	UINT8 n_u;
	UINT8 n_v;

	INT16 n_distance;
	UINT32 n_h;
	UINT16 *p_vram;
	UINT16 *p_clut;
	UINT16 n_bgr;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 10 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_x + 7, COORD_Y( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_y + 7 );
	DebugMesh( COORD_X( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_x + 7, COORD_Y( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_y + 7 );
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.Sprite8x8.n_bgr );

	n_clutx = ( m_packet.Sprite8x8.n_texture.w.h & 0x3f ) << 4;
	n_cluty = ( m_packet.Sprite8x8.n_texture.w.h >> 6 ) & 0x3ff;

	TEXTURESETUP( m_n_drawmode )

	n_r.w.h = BGR_R( m_packet.Sprite8x8.n_bgr ); n_r.w.l = 0;
	n_g.w.h = BGR_G( m_packet.Sprite8x8.n_bgr ); n_g.w.l = 0;
	n_b.w.h = BGR_B( m_packet.Sprite8x8.n_bgr ); n_b.w.l = 0;

	n_v = TEXTURE_V( m_packet.Sprite8x8.n_texture );
	n_y = COORD_Y( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_y;
	n_h = 8;

	while( n_h > 0 )
	{
		n_x = COORD_X( m_packet.Sprite8x8.n_coord ) + m_n_drawoffset_x;
		n_u = TEXTURE_U( m_packet.Sprite8x8.n_texture );

		n_distance = 8;
		if( n_distance > 0 && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_u += ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			TEXTURESPAN( INT16, (UINT32)n_u << 16, 1 << 16, (UINT32)n_v << 16, 0, 0xff, 0xff, 0, 0, 0 )
			{
				TEXTUREFILL( FLATTEXTUREDRECTANGLEUPDATE, n_u, n_v );
			}
		}
		n_v++;
		n_y++;
		n_h--;
	}
}

static void Sprite16x16( void )
{
	INT16 n_y;
	INT16 n_x;

	UINT8 n_cmd;

	UINT32 n_clutx;
	UINT32 n_cluty;

	UINT32 n_tp;
	UINT32 n_tx;
	UINT32 n_ty;
	UINT32 n_abr;
	UINT32 n_ti;

	UINT16 *p_n_f;
	UINT16 *p_n_redb;
	UINT16 *p_n_greenb;
	UINT16 *p_n_blueb;
	UINT16 *p_n_redtrans;
	UINT16 *p_n_greentrans;
	UINT16 *p_n_bluetrans;

	PAIR n_r;
	PAIR n_g;
	PAIR n_b;
	UINT8 n_u;
	UINT8 n_v;

	INT16 n_distance;
	UINT32 n_h;
	UINT16 *p_vram;
	UINT16 *p_clut;
	UINT16 n_bgr;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 11 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_x + 7, COORD_Y( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_y );
	DebugMesh( COORD_X( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_y + 7 );
	DebugMesh( COORD_X( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_x + 7, COORD_Y( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_y + 7 );
	DebugMeshEnd();
#endif

	n_cmd = BGR_C( m_packet.Sprite16x16.n_bgr );

	n_clutx = ( m_packet.Sprite16x16.n_texture.w.h & 0x3f ) << 4;
	n_cluty = ( m_packet.Sprite16x16.n_texture.w.h >> 6 ) & 0x3ff;

	TEXTURESETUP( m_n_drawmode )

	n_r.w.h = BGR_R( m_packet.Sprite16x16.n_bgr ); n_r.w.l = 0;
	n_g.w.h = BGR_G( m_packet.Sprite16x16.n_bgr ); n_g.w.l = 0;
	n_b.w.h = BGR_B( m_packet.Sprite16x16.n_bgr ); n_b.w.l = 0;

	n_v = TEXTURE_V( m_packet.Sprite16x16.n_texture );
	n_y = COORD_Y( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_y;
	n_h = 16;

	while( n_h > 0 )
	{
		n_x = COORD_X( m_packet.Sprite16x16.n_coord ) + m_n_drawoffset_x;
		n_u = TEXTURE_U( m_packet.Sprite16x16.n_texture );

		n_distance = 16;
		if( n_distance > 0 && n_y >= (INT32)m_n_drawarea_y1 && n_y <= (INT32)m_n_drawarea_y2 )
		{
			if( ( (INT32)m_n_drawarea_x1 - n_x ) > 0 )
			{
				n_u += ( m_n_drawarea_x1 - n_x );
				n_distance -= ( m_n_drawarea_x1 - n_x );
				n_x = m_n_drawarea_x1;
			}
			TEXTURESPAN( INT16, (UINT32)n_u << 16, 1 << 16, (UINT32)n_v << 16, 0, 0xff, 0xff, 0, 0, 0 )
			{
				TEXTUREFILL( FLATTEXTUREDRECTANGLEUPDATE, n_u, n_v );
			}
		}
		n_v++;
		n_y++;
		n_h--;
	}
}

static void Dot( void )
{
	INT32 n_x;
	INT32 n_y;
	UINT32 n_r;
	UINT32 n_g;
	UINT32 n_b;
	UINT16 *p_vram;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 12 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.Dot.vertex.n_coord ) + m_n_drawoffset_x, COORD_Y( m_packet.Dot.vertex.n_coord ) + m_n_drawoffset_y );
	DebugMeshEnd();
#endif

	n_r = BGR_R( m_packet.Dot.n_bgr );
	n_g = BGR_G( m_packet.Dot.n_bgr );
	n_b = BGR_B( m_packet.Dot.n_bgr );
	n_x = COORD_X( m_packet.Dot.vertex.n_coord ) + m_n_drawoffset_x;
	n_y = COORD_Y( m_packet.Dot.vertex.n_coord ) + m_n_drawoffset_y;

	if( (INT16)n_x >= (INT32)m_n_drawarea_x1 &&
		(INT16)n_y >= (INT32)m_n_drawarea_y1 &&
		(INT16)n_x <= (INT32)m_n_drawarea_x2 &&
		(INT16)n_y <= (INT32)m_n_drawarea_y2 )
	{
		p_vram = m_p_p_vram[ n_y ] + n_x;
		*( p_vram ) = m_p_n_redshade[ MID_LEVEL | n_r ] |
			m_p_n_greenshade[ MID_LEVEL | n_g ] |
			m_p_n_blueshade[ MID_LEVEL | n_b ];
	}
}

static void MoveImage( void )
{
	INT16 n_w;
	INT16 n_h;
	INT16 n_srcx;
	INT16 n_srcy;
	INT16 n_dsty;
	INT16 n_dstx;

#if defined( MAME_DEBUG )
	if( m_n_debugskip == 13 )
	{
		return;
	}
	DebugMesh( COORD_X( m_packet.MoveImage.vertex[ 1 ].n_coord ), COORD_Y( m_packet.MoveImage.vertex[ 1 ].n_coord ) );
	DebugMesh( COORD_X( m_packet.MoveImage.vertex[ 1 ].n_coord ) + SIZE_W( m_packet.MoveImage.n_size ), COORD_Y( m_packet.MoveImage.vertex[ 1 ].n_coord ) );
	DebugMesh( COORD_X( m_packet.MoveImage.vertex[ 1 ].n_coord ), COORD_Y( m_packet.MoveImage.vertex[ 1 ].n_coord ) + SIZE_H( m_packet.MoveImage.n_size ) );
	DebugMesh( COORD_X( m_packet.MoveImage.vertex[ 1 ].n_coord ) + SIZE_W( m_packet.MoveImage.n_size ), COORD_Y( m_packet.MoveImage.vertex[ 1 ].n_coord ) + SIZE_H( m_packet.MoveImage.n_size ) );
	DebugMeshEnd();
#endif

	n_srcy = COORD_Y( m_packet.MoveImage.vertex[ 0 ].n_coord );
	n_dsty = COORD_Y( m_packet.MoveImage.vertex[ 1 ].n_coord );
	n_h = SIZE_H( m_packet.MoveImage.n_size );

	while( n_h > 0 )
	{
		n_srcx = COORD_X( m_packet.MoveImage.vertex[ 0 ].n_coord );
		n_dstx = COORD_X( m_packet.MoveImage.vertex[ 1 ].n_coord );
		n_w = SIZE_W( m_packet.MoveImage.n_size );
		while( n_w > 0 )
		{
			*( m_p_p_vram[ n_dsty & 1023 ] + ( n_dstx & 1023 ) ) =
				*( m_p_p_vram[ n_srcy & 1023 ] + ( n_srcx & 1023 ) );
			n_srcx++;
			n_dstx++;
			n_w--;
		}
		n_srcy++;
		n_dsty++;
		n_h--;
	}
}

#undef m_packet
#undef m_n_twy
#undef m_n_twx
#undef m_n_twh
#undef m_n_tww
#undef m_n_drawarea_x1
#undef m_n_drawarea_y1
#undef m_n_drawarea_x2
#undef m_n_drawarea_y2
#undef m_n_drawoffset_x
#undef m_n_drawoffset_y
#undef m_n_drawmode
#undef m_n_gpustatus

enum
{
	PSX_DRAW_FLATPOLYGON3, PSX_DRAW_FLATPOLYGON4,
	PSX_DRAW_FLATTEXTUREDPOLYGON3, PSX_DRAW_FLATTEXTUREDPOLYGON4,
	PSX_DRAW_GOURAUDPOLYGON3, PSX_DRAW_GOURAUDPOLYGON4,
	PSX_DRAW_GOURAUDTEXTUREDPOLYGON3, PSX_DRAW_GOURAUDTEXTUREDPOLYGON4,
	PSX_DRAW_MONOCHROMELINE, PSX_DRAW_GOURAUDLINE,
	PSX_DRAW_FRAMEBUFFERRECTANGLE, PSX_DRAW_FLATRECTANGLE, PSX_DRAW_FLATTEXTUREDRECTANGLE,
	PSX_DRAW_SPRITE8X8, PSX_DRAW_SPRITE16X16, PSX_DRAW_DOT, PSX_DRAW_MOVEIMAGE,
	PSX_DRAW_STORE	/* pixels sent to VRAM (command 0xa0) */
};

static void psx_draw_run( int n_op )
{
	switch( n_op )
	{
	case PSX_DRAW_FLATPOLYGON3: FlatPolygon( 3 ); break;
	case PSX_DRAW_FLATPOLYGON4: FlatPolygon( 4 ); break;
	case PSX_DRAW_FLATTEXTUREDPOLYGON3: FlatTexturedPolygon( 3 ); break;
	case PSX_DRAW_FLATTEXTUREDPOLYGON4: FlatTexturedPolygon( 4 ); break;
	case PSX_DRAW_GOURAUDPOLYGON3: GouraudPolygon( 3 ); break;
	case PSX_DRAW_GOURAUDPOLYGON4: GouraudPolygon( 4 ); break;
	case PSX_DRAW_GOURAUDTEXTUREDPOLYGON3: GouraudTexturedPolygon( 3 ); break;
	case PSX_DRAW_GOURAUDTEXTUREDPOLYGON4: GouraudTexturedPolygon( 4 ); break;
	case PSX_DRAW_MONOCHROMELINE: MonochromeLine(); break;
	case PSX_DRAW_GOURAUDLINE: GouraudLine(); break;
	case PSX_DRAW_FRAMEBUFFERRECTANGLE: FrameBufferRectangleDraw(); break;
	case PSX_DRAW_FLATRECTANGLE: FlatRectangle(); break;
	case PSX_DRAW_FLATTEXTUREDRECTANGLE: FlatTexturedRectangle(); break;
	case PSX_DRAW_SPRITE8X8: Sprite8x8(); break;
	case PSX_DRAW_SPRITE16X16: Sprite16x16(); break;
	case PSX_DRAW_DOT: Dot(); break;
	case PSX_DRAW_MOVEIMAGE: MoveImage(); break;
	}
}

#if PSX_GPU_THREADS

#define PSX_GQ_SIZE ( 4096 )	/* queued primitives, a power of two */
#define PSX_GQ_STORE ( 16 )	/* pixels per PSX_DRAW_STORE entry */
#define PSX_GQ_SPIN ( 4096 )	/* polls before sleeping */

struct psx_gq_item
{
	int n_op;
	int n_count;
	union
	{
		struct psx_draw draw;
		struct
		{
			UINT32 n_pos[ PSX_GQ_STORE ];	/* y << 10 | x */
			UINT16 n_value[ PSX_GQ_STORE ];
		} store;
	} u;
};

static struct psx_gq_item *psx_gq;
static UINT32 psx_gq_head;	/* next entry to fill: written by the emulation thread only */
static UINT32 psx_gq_tail;	/* next entry to draw: written by the drawing thread only */
static int psx_gq_sleeping;	/* the drawing thread waits for work */
static int psx_gq_waiting;	/* the emulation thread waits for the drawing */
static int psx_gq_quit;
static int psx_gq_running;
static struct psx_gq_item *psx_gq_store;	/* open PSX_DRAW_STORE entry at psx_gq_head */
static pthread_t psx_gq_thread;
static pthread_mutex_t psx_gq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t psx_gq_work = PTHREAD_COND_INITIALIZER;
static pthread_cond_t psx_gq_done = PTHREAD_COND_INITIALIZER;

static void *psx_gq_main( void *p )
{
	UINT32 n_tail = psx_gq_tail;

	for( ;; )
	{
		UINT32 n_head = __atomic_load_n( &psx_gq_head, __ATOMIC_ACQUIRE );
		struct psx_gq_item *item;

		if( n_head == n_tail )
		{
			int n_spin;
			for( n_spin = 0; n_spin < PSX_GQ_SPIN && n_head == n_tail; n_spin++ )
			{
				n_head = __atomic_load_n( &psx_gq_head, __ATOMIC_ACQUIRE );
			}
			if( n_head == n_tail )
			{
				int b_quit;
				pthread_mutex_lock( &psx_gq_lock );
				__atomic_store_n( &psx_gq_sleeping, 1, __ATOMIC_SEQ_CST );
				while( ( n_head = __atomic_load_n( &psx_gq_head, __ATOMIC_SEQ_CST ) ) == n_tail && !psx_gq_quit )
				{
					pthread_cond_wait( &psx_gq_work, &psx_gq_lock );
				}
				__atomic_store_n( &psx_gq_sleeping, 0, __ATOMIC_SEQ_CST );
				b_quit = psx_gq_quit;
				pthread_mutex_unlock( &psx_gq_lock );
				if( n_head == n_tail && b_quit )
				{
					break;
				}
			}
			continue;
		}

		item = &psx_gq[ n_tail & ( PSX_GQ_SIZE - 1 ) ];
		if( item->n_op == PSX_DRAW_STORE )
		{
			int i;
			for( i = 0; i < item->n_count; i++ )
			{
				*( m_p_p_vram[ item->u.store.n_pos[ i ] >> 10 ] + ( item->u.store.n_pos[ i ] & 1023 ) ) = item->u.store.n_value[ i ];
			}
		}
		else
		{
			psx_d = &item->u.draw;
			psx_draw_run( item->n_op );
		}

		n_tail++;
		__atomic_store_n( &psx_gq_tail, n_tail, __ATOMIC_SEQ_CST );
		if( __atomic_load_n( &psx_gq_waiting, __ATOMIC_SEQ_CST ) )
		{
			pthread_mutex_lock( &psx_gq_lock );
			pthread_cond_signal( &psx_gq_done );
			pthread_mutex_unlock( &psx_gq_lock );
		}
	}
	return NULL;
}

/* wait until the drawing thread has taken every entry before n_target */
static void psx_gq_wait( UINT32 n_target )
{
	int n_spin;
	for( n_spin = 0; n_spin < PSX_GQ_SPIN; n_spin++ )
	{
		if( (INT32)( __atomic_load_n( &psx_gq_tail, __ATOMIC_ACQUIRE ) - n_target ) >= 0 )
		{
			return;
		}
	}
	pthread_mutex_lock( &psx_gq_lock );
	__atomic_store_n( &psx_gq_waiting, 1, __ATOMIC_SEQ_CST );
	while( (INT32)( __atomic_load_n( &psx_gq_tail, __ATOMIC_SEQ_CST ) - n_target ) < 0 )
	{
		pthread_cond_wait( &psx_gq_done, &psx_gq_lock );
	}
	__atomic_store_n( &psx_gq_waiting, 0, __ATOMIC_SEQ_CST );
	pthread_mutex_unlock( &psx_gq_lock );
}

/* the entry at psx_gq_head, once there is room for it */
static struct psx_gq_item *psx_gq_slot( void )
{
	if( psx_gq_head - __atomic_load_n( &psx_gq_tail, __ATOMIC_ACQUIRE ) >= PSX_GQ_SIZE )
	{
		psx_gq_wait( psx_gq_head - PSX_GQ_SIZE / 2 );
	}
	return &psx_gq[ psx_gq_head & ( PSX_GQ_SIZE - 1 ) ];
}

/* hand the entry at psx_gq_head to the drawing thread */
static void psx_gq_push( void )
{
	__atomic_store_n( &psx_gq_head, psx_gq_head + 1, __ATOMIC_SEQ_CST );
	if( __atomic_load_n( &psx_gq_sleeping, __ATOMIC_SEQ_CST ) )
	{
		pthread_mutex_lock( &psx_gq_lock );
		pthread_cond_signal( &psx_gq_work );
		pthread_mutex_unlock( &psx_gq_lock );
	}
}

static void psx_gq_flush_store( void )
{
	if( psx_gq_store != NULL )
	{
		psx_gq_store = NULL;
		psx_gq_push();
	}
}

static void psx_gq_store_pixel( UINT32 n_y, UINT32 n_x, UINT16 n_value )
{
	if( psx_gq_store == NULL )
	{
		psx_gq_store = psx_gq_slot();
		psx_gq_store->n_op = PSX_DRAW_STORE;
		psx_gq_store->n_count = 0;
	}
	psx_gq_store->u.store.n_pos[ psx_gq_store->n_count ] = ( n_y << 10 ) | n_x;
	psx_gq_store->u.store.n_value[ psx_gq_store->n_count ] = n_value;
	psx_gq_store->n_count++;
	if( psx_gq_store->n_count == PSX_GQ_STORE )
	{
		psx_gq_flush_store();
	}
}

static void psx_gq_start( void )
{
	if( psx_gq_running )
	{
		return;
	}
	psx_gq = malloc( PSX_GQ_SIZE * sizeof( *psx_gq ) );
	if( psx_gq == NULL )
	{
		return;
	}
	psx_gq_head = 0;
	psx_gq_tail = 0;
	psx_gq_sleeping = 0;
	psx_gq_waiting = 0;
	psx_gq_quit = 0;
	psx_gq_store = NULL;
	if( pthread_create( &psx_gq_thread, NULL, psx_gq_main, NULL ) != 0 )
	{
		free( psx_gq );
		psx_gq = NULL;
		return;
	}
	psx_gq_running = 1;
}

/* everything queued drawn: VRAM can be read */
static void psx_gpu_sync( void )
{
	if( psx_gq_running )
	{
		psx_gq_flush_store();
		psx_gq_wait( psx_gq_head );
	}
}

static void psx_gq_stop( void )
{
	if( !psx_gq_running )
	{
		return;
	}
	psx_gpu_sync();
	pthread_mutex_lock( &psx_gq_lock );
	psx_gq_quit = 1;
	pthread_cond_signal( &psx_gq_work );
	pthread_mutex_unlock( &psx_gq_lock );
	pthread_join( psx_gq_thread, NULL );
	free( psx_gq );
	psx_gq = NULL;
	psx_gq_running = 0;
}

int psx_gpu_thread_available( void )
{
	return 1;
}

#else

#define psx_gq_running ( 0 )
static void psx_gpu_sync( void ) {}
static void psx_gq_flush_store( void ) {}
static void psx_gq_start( void ) {}
static void psx_gq_stop( void ) {}

int psx_gpu_thread_available( void )
{
	return 0;
}

#endif

/* the driver asks for the drawing thread (before VIDEO_START) */
void psx_gpu_set_thread( int b_enable )
{
	psx_gpu_thread_wanted = b_enable;
}

/* a textured polygon takes its texture page into the draw mode (TEXTURESETUP) */
static void psx_gpu_texpage( UINT32 n_drawmode )
{
	if( m_n_drawmode != n_drawmode )
	{
		m_n_drawmode = n_drawmode;
		if( m_n_gputype == 2 )
		{
			m_n_gpustatus = ( m_n_gpustatus & 0xfffff800 ) | ( m_n_drawmode & 0x7ff );
		}
		else
		{
			m_n_gpustatus = ( m_n_gpustatus & 0xffffe000 ) | ( m_n_drawmode & 0x1fff );
		}
	}
}

/* draw the primitive in m_packet with the current state, now or on the drawing thread */
static void psx_gpu_draw( int n_op )
{
	static struct psx_draw local;
	struct psx_draw *d = &local;
#if PSX_GPU_THREADS
	struct psx_gq_item *item = NULL;

	if( psx_gq_running )
	{
		psx_gq_flush_store();
		item = psx_gq_slot();
		item->n_op = n_op;
		d = &item->u.draw;
	}
#endif

	d->packet = m_packet;
	d->twy = m_n_twy;
	d->twx = m_n_twx;
	d->twh = m_n_twh;
	d->tww = m_n_tww;
	d->drawarea_x1 = m_n_drawarea_x1;
	d->drawarea_y1 = m_n_drawarea_y1;
	d->drawarea_x2 = m_n_drawarea_x2;
	d->drawarea_y2 = m_n_drawarea_y2;
	d->drawoffset_x = m_n_drawoffset_x;
	d->drawoffset_y = m_n_drawoffset_y;
	d->drawmode = m_n_drawmode;
	d->gpustatus = m_n_gpustatus;

	switch( n_op )
	{
	case PSX_DRAW_FLATTEXTUREDPOLYGON3:
	case PSX_DRAW_FLATTEXTUREDPOLYGON4:
		psx_gpu_texpage( m_packet.FlatTexturedPolygon.vertex[ 1 ].n_texture.w.h );
		break;
	case PSX_DRAW_GOURAUDTEXTUREDPOLYGON3:
	case PSX_DRAW_GOURAUDTEXTUREDPOLYGON4:
		psx_gpu_texpage( m_packet.GouraudTexturedPolygon.vertex[ 1 ].n_texture.w.h );
		break;
	}

#if PSX_GPU_THREADS
	if( item != NULL )
	{
		psx_gq_push();
		return;
	}
#endif
	psx_d = d;
	psx_draw_run( n_op );
}

/* End of a node of a DMA linked list (GPU channel). A drawing command still
   waiting for words there is dropped, instead of taking the words of the next
   node as its own: that throws the parser out of step for the rest of the list,
   and the misread words become fills and copies all over VRAM (Soul Edge, after a
   few minutes: textures wiped, huge fills slowing everything down).
   The emulated GPU draws at once, so the games can rebuild a primitive between
   the time they link it and the time the list is sent; on the real board the
   drawing time keeps them apart. Transfers to VRAM (0xa0) are left alone. */
void psx_gpu_list_node_end( void )
{
	if( m_n_gpu_buffer_offset != 0 )
	{
		int n_command = m_packet.n_entry[ 0 ] >> 24;
		if( n_command >= 0x20 && n_command < 0x80 )
		{
			verboselog( 1, "dropped incomplete command %02x (%u words)\n", n_command, m_n_gpu_buffer_offset );
			m_n_gpu_buffer_offset = 0;
		}
	}
}

void psx_gpu_write( UINT32 *p_ram, INT32 n_size )
{
	while( n_size > 0 )
	{
		UINT32 data = *( p_ram );

		verboselog( 2, "PSX Packet #%u %08x\n", m_n_gpu_buffer_offset, data );
		m_packet.n_entry[ m_n_gpu_buffer_offset ] = data;
		switch( m_packet.n_entry[ 0 ] >> 24 )
		{
		case 0x00:
		case 0x01:
			break;
		case 0x02:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: frame buffer rectangle %u,%u %u,%u\n", m_packet.n_entry[ 0 ] >> 24,
					m_packet.n_entry[ 1 ] & 0xffff, m_packet.n_entry[ 1 ] >> 16, m_packet.n_entry[ 2 ] & 0xffff, m_packet.n_entry[ 2 ] >> 16 );
#endif
				psx_gpu_draw( PSX_DRAW_FRAMEBUFFERRECTANGLE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: monochrome 3 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_FLATPOLYGON3 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
			if( m_n_gpu_buffer_offset < 6 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: textured 3 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_FLATTEXTUREDPOLYGON3 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x28:
		case 0x29:
		case 0x2a:
		case 0x2b:
			if( m_n_gpu_buffer_offset < 4 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: monochrome 4 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_FLATPOLYGON4 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x2c:
		case 0x2d:
		case 0x2e:
		case 0x2f:
			if( m_n_gpu_buffer_offset < 8 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: textured 4 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_FLATTEXTUREDPOLYGON4 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			if( m_n_gpu_buffer_offset < 5 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud 3 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDPOLYGON3 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
			if( m_n_gpu_buffer_offset < 8 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud textured 3 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDTEXTUREDPOLYGON3 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x38:
		case 0x39:
		case 0x3a:
		case 0x3b:
			if( m_n_gpu_buffer_offset < 7 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud 4 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDPOLYGON4 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x3c:
		case 0x3d:
		case 0x3e:
		case 0x3f:
			if( m_n_gpu_buffer_offset < 11 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud textured 4 point polygon\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDTEXTUREDPOLYGON4 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x40:
		case 0x41:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: monochrome line\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_MONOCHROMELINE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x48:
		case 0x4c:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: monochrome polyline\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_MONOCHROMELINE );
				if( ( m_packet.n_entry[ 3 ] & 0xf000f000 ) != 0x50005000 )
				{
					m_packet.n_entry[ 1 ] = m_packet.n_entry[ 2 ];
					m_packet.n_entry[ 2 ] = m_packet.n_entry[ 3 ];
					m_n_gpu_buffer_offset = 3;
				}
				else
				{
					m_n_gpu_buffer_offset = 0;
				}
			}
			break;
		case 0x50:
		case 0x52:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud line\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDLINE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x58:
		case 0x5c:
			if( m_n_gpu_buffer_offset < 5 &&
				( m_n_gpu_buffer_offset != 4 || ( m_packet.n_entry[ 4 ] & 0xf000f000 ) != 0x50005000 ) )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				/*verboselog( 1, "%02x: gouraud polyline\n", m_packet.n_entry[ 0 ] >> 24 );*/
				psx_gpu_draw( PSX_DRAW_GOURAUDLINE );
				if( ( m_packet.n_entry[ 4 ] & 0xf000f000 ) != 0x50005000 )
				{
					m_packet.n_entry[ 0 ] = ( m_packet.n_entry[ 0 ] & 0xff000000 ) | ( m_packet.n_entry[ 2 ] & 0x00ffffff );
					m_packet.n_entry[ 1 ] = m_packet.n_entry[ 3 ];
					m_packet.n_entry[ 2 ] = m_packet.n_entry[ 4 ];
					m_packet.n_entry[ 3 ] = m_packet.n_entry[ 5 ];
					m_n_gpu_buffer_offset = 4;
				}
				else
				{
					m_n_gpu_buffer_offset = 0;
				}
			}
			break;
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: rectangle %d,%d %d,%d\n",
					m_packet.n_entry[ 0 ] >> 24,
					(INT16)( m_packet.n_entry[ 1 ] & 0xffff ), (INT16)( m_packet.n_entry[ 1 ] >> 16 ),
					(INT16)( m_packet.n_entry[ 2 ] & 0xffff ), (INT16)( m_packet.n_entry[ 2 ] >> 16 ) );
#endif
				psx_gpu_draw( PSX_DRAW_FLATRECTANGLE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: sprite %d,%d %u,%u %08x, %08x\n",
					m_packet.n_entry[ 0 ] >> 24,
					(INT16)( m_packet.n_entry[ 1 ] & 0xffff ), (INT16)( m_packet.n_entry[ 1 ] >> 16 ),
					m_packet.n_entry[ 3 ] & 0xffff, m_packet.n_entry[ 3 ] >> 16,
					m_packet.n_entry[ 0 ], m_packet.n_entry[ 2 ] );
#endif
				psx_gpu_draw( PSX_DRAW_FLATTEXTUREDRECTANGLE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x68:
			if( m_n_gpu_buffer_offset < 1 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: dot %d,%d %08x\n",
					m_packet.n_entry[ 0 ] >> 24,
					(INT16)( m_packet.n_entry[ 1 ] & 0xffff ), (INT16)( m_packet.n_entry[ 1 ] >> 16 ),
					m_packet.n_entry[ 0 ] & 0xffffff );
#endif
				psx_gpu_draw( PSX_DRAW_DOT );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x74:
		case 0x77:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: 8x8 sprite %08x %08x %08x\n", m_packet.n_entry[ 0 ] >> 24,
					m_packet.n_entry[ 0 ], m_packet.n_entry[ 1 ], m_packet.n_entry[ 2 ] );
#endif
				psx_gpu_draw( PSX_DRAW_SPRITE8X8 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x7c:
		case 0x7f:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: 16x16 sprite %08x %08x %08x\n", m_packet.n_entry[ 0 ] >> 24,
					m_packet.n_entry[ 0 ], m_packet.n_entry[ 1 ], m_packet.n_entry[ 2 ] );
#endif
				psx_gpu_draw( PSX_DRAW_SPRITE16X16 );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0x80:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "move image in frame buffer %08x %08x %08x %08x\n", m_packet.n_entry[ 0 ], m_packet.n_entry[ 1 ], m_packet.n_entry[ 2 ], m_packet.n_entry[ 3 ] );
#endif
				psx_gpu_draw( PSX_DRAW_MOVEIMAGE );
				m_n_gpu_buffer_offset = 0;
			}
			break;
		case 0xa0:
			if( m_n_gpu_buffer_offset < 3 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
				UINT32 n_pixel;
				for( n_pixel = 0; n_pixel < 2; n_pixel++ )
				{
#if 0
					verboselog( 2, "send image to framebuffer ( pixel %u,%u = %u )\n",
						( m_n_vramx + m_packet.n_entry[ 1 ] ) & 1023,
						( m_n_vramy + ( m_packet.n_entry[ 1 ] >> 16 ) ) & 1023,
						data & 0xffff );
#endif

#if PSX_GPU_THREADS
					if( psx_gq_running )
					{
						psx_gq_store_pixel( ( m_n_vramy + ( m_packet.n_entry[ 1 ] >> 16 ) ) & 1023, ( m_n_vramx + m_packet.n_entry[ 1 ] ) & 1023, data & 0xffff );
					}
					else
#endif
					*( m_p_p_vram[ ( m_n_vramy + ( m_packet.n_entry[ 1 ] >> 16 ) ) & 1023 ] + ( ( m_n_vramx + m_packet.n_entry[ 1 ] ) & 1023 ) ) = data & 0xffff;
					m_n_vramx++;
					if( m_n_vramx >= ( m_packet.n_entry[ 2 ] & 0xffff ) )
					{
						m_n_vramx = 0;
						m_n_vramy++;
						if( m_n_vramy >= ( m_packet.n_entry[ 2 ] >> 16 ) )
						{
#if 0
							verboselog( 1, "%02x: send image to framebuffer %u,%u %u,%u\n", m_packet.n_entry[ 0 ] >> 24,
								m_packet.n_entry[ 1 ] & 0xffff, ( m_packet.n_entry[ 1 ] >> 16 ),
								m_packet.n_entry[ 2 ] & 0xffff, ( m_packet.n_entry[ 2 ] >> 16 ) );
#endif
							m_n_gpu_buffer_offset = 0;
							m_n_vramx = 0;
							m_n_vramy = 0;
							break;
						}
					}
					data >>= 16;
				}
			}
			break;
		case 0xc0:
			if( m_n_gpu_buffer_offset < 2 )
			{
				m_n_gpu_buffer_offset++;
			}
			else
			{
#if 0
				verboselog( 1, "%02x: copy image from frame buffer\n", m_packet.n_entry[ 0 ] >> 24 );
#endif
				m_n_gpustatus |= ( 1L << 0x1b );
			}
			break;
		case 0xe1:
#if 0
			verboselog( 1, "%02x: draw mode %06x\n", m_packet.n_entry[ 0 ] >> 24,
				m_packet.n_entry[ 0 ] & 0xffffff );
#endif
			m_n_drawmode = m_packet.n_entry[ 0 ] & 0xffffff;
			if( m_n_gputype == 2 )
			{
				m_n_gpustatus = ( m_n_gpustatus & 0xfffff800 ) | ( m_n_drawmode & 0x7ff );
			}
			else
			{
				m_n_gpustatus = ( m_n_gpustatus & 0xffffe000 ) | ( m_n_drawmode & 0x1fff );
			}
			break;
		case 0xe2:
			m_n_twy = ( ( ( m_packet.n_entry[ 0 ] >> 15 ) & 0x1f ) << 3 );
			m_n_twx = ( ( ( m_packet.n_entry[ 0 ] >> 10 ) & 0x1f ) << 3 );
			m_n_twh = 256 - ( ( ( m_packet.n_entry[ 0 ] >> 5 ) & 0x1f ) << 3 );
			m_n_tww = 256 - ( ( m_packet.n_entry[ 0 ] & 0x1f ) << 3 );
#if 0
			verboselog( 1, "%02x: texture window %u,%u %u,%u\n", m_packet.n_entry[ 0 ] >> 24,
				m_n_twx, m_n_twy, m_n_tww, m_n_twh );
#endif
			break;
		case 0xe3:
			m_n_drawarea_x1 = m_packet.n_entry[ 0 ] & 1023;
			if( m_n_gputype == 2 )
			{
				m_n_drawarea_y1 = ( m_packet.n_entry[ 0 ] >> 10 ) & 1023;
			}
			else
			{
				m_n_drawarea_y1 = ( m_packet.n_entry[ 0 ] >> 12 ) & 1023;
			}
#if 0
			verboselog( 1, "%02x: drawing area top left %d,%d\n", m_packet.n_entry[ 0 ] >> 24,
				m_n_drawarea_x1, m_n_drawarea_y1 );
#endif
			break;
		case 0xe4:
			m_n_drawarea_x2 = m_packet.n_entry[ 0 ] & 1023;
			if( m_n_gputype == 2 )
			{
				m_n_drawarea_y2 = ( m_packet.n_entry[ 0 ] >> 10 ) & 1023;
			}
			else
			{
				m_n_drawarea_y2 = ( m_packet.n_entry[ 0 ] >> 12 ) & 1023;
			}
#if 0
			verboselog( 1, "%02x: drawing area bottom right %d,%d\n", m_packet.n_entry[ 0 ] >> 24,
				m_n_drawarea_x2, m_n_drawarea_y2 );
#endif
			break;
		case 0xe5:
			m_n_drawoffset_x = SINT11( m_packet.n_entry[ 0 ] & 2047 );
			if( m_n_gputype == 2 )
			{
				m_n_drawoffset_y = SINT11( ( m_packet.n_entry[ 0 ] >> 11 ) & 2047 );
			}
			else
			{
				m_n_drawoffset_y = SINT11( ( m_packet.n_entry[ 0 ] >> 12 ) & 2047 );
			}
#if 0
			verboselog( 1, "%02x: drawing offset %d,%d\n", m_packet.n_entry[ 0 ] >> 24,
				m_n_drawoffset_x, m_n_drawoffset_y );
#endif
			break;
		case 0xe6:
#if 0
			if( ( m_packet.n_entry[ 0 ] & 3 ) != 0 )
			{
				verboselog( 1, "not handled: mask setting %d\n", m_packet.n_entry[ 0 ] & 3 );
			}
			else
			{
				verboselog( 1, "mask setting %d\n", m_packet.n_entry[ 0 ] & 3 );
			}
#endif
			break;
		default:
#if defined( MAME_DEBUG )
			usrintf_showmessage_secs( 1, "unknown GPU packet %08x", m_packet.n_entry[ 0 ] );
#endif
#if 0
			verboselog( 0, "unknown GPU packet %08x (%08x)\n", m_packet.n_entry[ 0 ], data );
#endif
#if ( STOP_ON_ERROR )
			m_n_gpu_buffer_offset = 1;
#endif
			break;
		}
		p_ram++;
		n_size--;
	}
	psx_gq_flush_store();
}

WRITE32_HANDLER( psx_gpu_w )
{
	switch( offset )
	{
	case 0x00:
		psx_gpu_write( &data, 1 );
		break;
	case 0x01:
		switch( data >> 24 )
		{
		case 0x00:
			verboselog( 1, "reset gpu\n" );
			m_n_gpu_buffer_offset = 0;
			m_n_gpustatus = 0x14802000;
			m_n_drawmode = 0;
			m_n_drawarea_x1 = 0;
			m_n_drawarea_y1 = 0;
			m_n_drawarea_x2 = 1023;
			m_n_drawarea_y2 = 1023;
			m_n_drawoffset_x = 0;
			m_n_drawoffset_y = 0;
			m_n_displaystartx = 0;
			m_n_displaystarty = 0;
			m_n_screenwidth = 256;
			m_n_screenheight = 240;
			m_n_vramx = 0;
			m_n_vramy = 0;
			m_n_twx = 0;
			m_n_twy = 0;
			m_n_twh = 255;
			m_n_tww = 255;
			break;
		case 0x01:
			verboselog( 1, "not handled: reset command buffer\n" );
			m_n_gpu_buffer_offset = 0;
			break;
		case 0x02:
			verboselog( 1, "not handled: reset irq\n" );
			break;
		case 0x03:
			m_n_gpustatus &= ~( 1L << 0x17 );
			m_n_gpustatus |= ( data & 0x01 ) << 0x17;
			break;
		case 0x04:
			verboselog( 1, "dma setup %d\n", data & 3 );
			m_n_gpustatus &= ~( 3L << 0x1d );
			m_n_gpustatus |= ( data & 0x03 ) << 0x1d;
			break;
		case 0x05:
			m_n_displaystartx = data & 1023;
			if( m_n_gputype == 2 )
			{
				m_n_displaystarty = ( data >> 10 ) & 1023;
			}
			else
			{
				m_n_displaystarty = ( data >> 12 ) & 1023;
			}
			/*verboselog( 1, "start of display area %d %d\n", m_n_displaystartx, m_n_displaystarty );*/
			break;
		case 0x06:
			m_n_horiz_disstart = data & 4095;
			m_n_horiz_disend = ( data >> 12 ) & 4095;
			/*verboselog( 1, "horizontal display range %d %d\n", m_n_horiz_disstart, m_n_horiz_disend );*/
			break;
		case 0x07:
			m_n_vert_disstart = data & 1023;
			m_n_vert_disend = ( data >> 10 ) & 2047;
			/*verboselog( 1, "vertical display range %d %d\n", m_n_vert_disstart, m_n_vert_disend );*/
			break;
		case 0x08:
			/*verboselog( 1, "display mode %02x\n", data & 0xff );*/
			m_n_gpustatus &= ~( 127L << 0x10 );
			m_n_gpustatus |= ( data & 0x3f ) << 0x11; /* width 0 + height + videmode + isrgb24 + isinter */
			m_n_gpustatus |= ( ( data & 0x40 ) >> 0x06 ) << 0x10; /* width 1 */
			m_b_reverseflag = ( data >> 7 ) & 1;
			switch( ( m_n_gpustatus >> 0x13 ) & 1 )
			{
			case 0:
				m_n_screenheight = 240;
				break;
			case 1:
				m_n_screenheight = 480;
				break;
			}
			switch( ( m_n_gpustatus >> 0x11 ) & 3 )
			{
			case 0:
				switch( ( m_n_gpustatus >> 0x10 ) & 1 )
				{
				case 0:
					m_n_screenwidth = 256;
					break;
				case 1:
					m_n_screenwidth = 384;
					break;
				}
				break;
			case 1:
				m_n_screenwidth = 320;
				break;
			case 2:
				m_n_screenwidth = 512;
				break;
			case 3:
				m_n_screenwidth = 640;
				break;
			}
			break;
		case 0x09:
			/*verboselog( 1, "not handled: GPU Control 0x09: %08x\n", data );*/
			break;
		case 0x10:
			switch( data & 7 )
			{
			case 0x03:
				verboselog( 1, "GPU Info - Draw area top left\n" );
				if( m_n_gputype == 2 )
				{
					m_n_gpuinfo = m_n_drawarea_x1 | ( m_n_drawarea_y1 << 10 );
				}
				else
				{
					m_n_gpuinfo = m_n_drawarea_x1 | ( m_n_drawarea_y1 << 12 );
				}
				break;
			case 0x04:
				verboselog( 1, "GPU Info - Draw area bottom right\n" );
				if( m_n_gputype == 2 )
				{
					m_n_gpuinfo = m_n_drawarea_x2 | ( m_n_drawarea_y2 << 10 );
				}
				else
				{
					m_n_gpuinfo = m_n_drawarea_x2 | ( m_n_drawarea_y2 << 12 );
				}
				break;
			case 0x05:
				verboselog( 1, "GPU Info - Draw offset\n" );
				if( m_n_gputype == 2 )
				{
					m_n_gpuinfo = ( m_n_drawoffset_x & 2047 ) | ( ( m_n_drawoffset_y & 2047 ) << 11 );
				}
				else
				{
					m_n_gpuinfo = ( m_n_drawoffset_x & 2047 ) | ( ( m_n_drawoffset_y & 2047 ) << 12 );
				}
				break;
			case 0x07:
				verboselog( 1, "GPU Info - GPU Type\n" );
				m_n_gpuinfo = m_n_gputype;
				break;
			default:
				verboselog( 0, "GPU Info - unknown request (%08x)\n", data );
				m_n_gpuinfo = 0;
				break;
			}
			break;
		case 0x20:
			verboselog( 1, "not handled: GPU Control 0x20: %08x\n", data );
			break;
		default:
#if defined( MAME_DEBUG )
			usrintf_showmessage_secs( 1, "unknown GPU command %08x", data );
#endif
			verboselog( 0, "gpu_w( %08x ) unknown GPU command\n", data );
			break;
		}
		break;
	default:
		verboselog( 0, "gpu_w( %08x, %08x, %08x ) unknown register\n", offset, data, mem_mask );
		break;
	}
}


void psx_gpu_read( UINT32 *p_ram, INT32 n_size )
{
	if( ( m_n_gpustatus & ( 1L << 0x1b ) ) != 0 )
	{
		psx_gpu_sync();
	}
	while( n_size > 0 )
	{
		if( ( m_n_gpustatus & ( 1L << 0x1b ) ) != 0 )
		{
			UINT32 n_pixel;
			PAIR data;

			/*verboselog( 2, "copy image from frame buffer ( %d, %d )\n", m_n_vramx, m_n_vramy );*/
			data.d = 0;
			for( n_pixel = 0; n_pixel < 2; n_pixel++ )
			{
				data.w.l = data.w.h;
				data.w.h = *( m_p_p_vram[ m_n_vramy + ( m_packet.n_entry[ 1 ] >> 16 ) ] + m_n_vramx + ( m_packet.n_entry[ 1 ] & 0xffff ) );
				m_n_vramx++;
				if( m_n_vramx >= ( m_packet.n_entry[ 2 ] & 0xffff ) )
				{
					m_n_vramx = 0;
					m_n_vramy++;
					if( m_n_vramy >= ( m_packet.n_entry[ 2 ] >> 16 ) )
					{
						/*verboselog( 1, "copy image from frame buffer end\n" );*/
						m_n_gpustatus &= ~( 1L << 0x1b );
						m_n_gpu_buffer_offset = 0;
						m_n_vramx = 0;
						m_n_vramy = 0;
						if( n_pixel == 0 )
						{
							data.w.l = data.w.h;
							data.w.h = 0;
						}
						break;
					}
				}
			}
			*( p_ram ) = data.d;
		}
		else
		{
			/*verboselog( 2, "read GPU info (%08x)\n", m_n_gpuinfo );*/
			*( p_ram ) = m_n_gpuinfo;
		}
		p_ram++;
		n_size--;
	}
}

READ32_HANDLER( psx_gpu_r )
{
	UINT32 data;

	switch( offset )
	{
	case 0x00:
		psx_gpu_read( &data, 1 );
		break;
	case 0x01:
		/*verboselog( 1, "read GPU status (%08x)\n", m_n_gpustatus );*/
		data = m_n_gpustatus;
		break;
	default:
		/*verboselog( 0, "gpu_r( %08x, %08x ) unknown register\n", offset, mem_mask );*/
		data = 0;
		break;
	}
	return data;
}

INTERRUPT_GEN( psx_vblank )
{
	/* the frame drawn before the screen update and the end of retro_run */
	psx_gpu_sync();
	m_n_gpustatus ^= ( 1L << 31 );
	psx_irq_set( 0x0001 );
}

void psx_gpu_reset( void )
{
	psx_gpu_w( 1, 0, 0 );
}
