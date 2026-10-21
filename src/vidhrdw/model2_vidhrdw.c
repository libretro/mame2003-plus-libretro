/***************************************************************************

    Sega Model 2 video: System 24 tilemaps, geometrizer, rasterizer.

    From current MAME (src/mame/sega/model2_v.cpp, model2rd.ipp; copyright
    R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels,
    BSD-3-Clause), in C for MAME 2003. The geometrizer is MAME's HLE of the
    Model 2/2A DSP; the polygons go through the same Z-sort, clipping and
    texture/luma/colour pipeline, drawn by a scanline rasterizer here in
    place of MAME's poly_manager.

***************************************************************************/

#include <math.h>
#include <float.h>
#include "driver.h"
#include "state.h"
#include "vidhrdw/segaic24.h"
#include "includes/model2.h"

#define CLAMPF( v, lo, hi )	( ( v ) < ( lo ) ? ( lo ) : ( ( v ) > ( hi ) ? ( hi ) : ( v ) ) )

static INLINE float u2f( UINT32 v ) { union { float f; UINT32 u; } x; x.u = v; return x.f; }
static INLINE UINT32 f2u( float v ) { union { float f; UINT32 u; } x; x.f = v; return x.u; }

static void m2_error( const char *msg )
{
	logerror( "%s", msg );
}


/*************************************
 *  Data types (model2.h of current MAME)
 *************************************/

typedef struct
{
	float x, y;
	float p[ 4 ];
} poly_vertex;

#define pz		p[0]
#define pu		p[1]
#define pv		p[2]

typedef struct
{
	poly_vertex normal;
	float distance;
} plane;

typedef struct
{
	float diffuse;
	float ambient;
	UINT32 specular_control;
	float specular_scale;
} texture_parameter;

typedef struct polygon
{
	struct polygon *next;
	poly_vertex v[ 8 ];
	UINT8 num_vertices;
	UINT16 z;
	UINT16 texheader[ 4 ];
	UINT8 luma;
	INT32 texlod;
	INT16 viewport[ 4 ];
	INT16 center[ 2 ];
	UINT8 window;
} polygon;

typedef struct
{
	poly_vertex v[ 4 ];
	UINT16 z;
	UINT16 texheader[ 4 ];
	UINT8 luma;
	INT32 texlod;
} quad_m2;

#define MAX_POLYGONS		32768

typedef struct
{
	UINT16 *texture_rom;
	UINT32 texture_rom_mask;
	INT16 viewport[ 4 ];
	INT16 center[ 4 ][ 2 ];
	UINT16 center_sel;
	UINT32 reverse;
	INT32 z_adjust;
	float polygon_z;
	UINT8 master_z_clip;
	UINT32 cur_command;
	UINT32 command_buffer[ 32 ];
	UINT32 command_index;
	polygon *poly_list;
	UINT32 poly_list_index;
	polygon **poly_sorted_list;
	UINT16 min_z;
	UINT16 max_z;
	UINT16 texture_ram[ 0x10000 ];
	UINT8 log_ram[ 0x8000 ];
	UINT8 cur_window;
	plane clip_plane[ 4 ][ 4 ];
} raster_state;

typedef struct
{
	raster_state *raster;
	UINT32 mode;
	UINT32 *polygon_rom;
	UINT32 polygon_rom_mask;
	float matrix[ 12 ];
	float matrix2[ 12 ];	/* Top Skater: the second matrix (command 1B) */
	int compact;	/* object data with no normal and no unused point slots */
	poly_vertex focus;
	poly_vertex light;
	float lod;
	float coef_table[ 32 ];
	texture_parameter texture_parameters[ 32 ];
	UINT32 polygon_ram0[ 0x8000 ];
	UINT32 polygon_ram1[ 0x8000 ];
} geo_state;

static raster_state *m_raster;
static geo_state *m_geo;
static int m_render_done;

static void render_frame_start( void )
{
	raster_state *raster = m_raster;

	raster->poly_list_index = 0;
	memset( raster->poly_sorted_list, 0, 0x10000 * sizeof( polygon * ) );
	raster->min_z = 0xffff;
	raster->max_z = 0;
	/* Zero Gunner sets backgrounds with "previous z value" mode at the start
	   of the display list, needs this to be this big */
	raster->polygon_z = 1e10;
	raster->cur_window = 0;
	m_render_done = 0;
}

/*************************************
 *  Geometrizer registers (i960 side)
 *************************************/

static UINT32 m_geoctl, m_geocnt;
static UINT32 m_geo_write_start_address, m_geo_read_start_address;
static int m_crtc_xoffset, m_crtc_yoffset;

static void push_geo_data( UINT32 data )
{
	model2_bufferram[ ( m_geo_write_start_address / 4 ) & 0x7fff ] = data;
	m_geo_write_start_address += 4;
}

WRITE32_HANDLER( model2_geo_ctl1_w )
{
	if( ( data ^ m_geoctl ) == 0x80000000 && ( data & 0x80000000 ) )
		m_geocnt = 0;
	m_geoctl = data;
}

READ32_HANDLER( model2_geo_prg_r )
{
	return 0xffffffff;
}

WRITE32_HANDLER( model2_geo_prg_w )
{
	if( m_geoctl & 0x80000000 )
		m_geocnt++;
	else
		push_geo_data( data );
}

READ32_HANDLER( model2_geo_r )
{
	int address = offset * 4;
	if( address == 0x2008 )
		return m_geo_write_start_address;
	else if( address == 0x3008 )
		return m_geo_read_start_address;
	return 0;
}

WRITE32_HANDLER( model2_geo_w )
{
	int address = offset * 4;

	if( address < 0x1000 )
	{
		if( data & 0x80000000 )
		{
			UINT32 r = data & 0x800fffff;
			r |= ( ( address >> 4 ) & 0x3f ) << 23;
			push_geo_data( r );
		}
		else if( ( address & 0xf ) == 0 )
		{
			UINT32 r = data & 0x000fffff;
			r |= ( ( address >> 4 ) & 0x3f ) << 23;
			if( ( address >> 4 ) & 0xc0 )
			{
				UINT8 function = ( address >> 4 ) & 0x3f;
				if( function == 1 )
					r |= ( ( address >> 10 ) & 3 ) << 29;	/* eye mode */
			}
			push_geo_data( r );
		}
	}
	else if( address == 0x1008 )
		m_geo_write_start_address = data & 0xfffff;
	else if( address == 0x3008 )
		m_geo_read_start_address = data & 0xfffff;
}

void model2_geo_reset( void )
{
	m_geoctl = 0;
	m_geocnt = 0;
	m_geo_write_start_address = 0;
	m_geo_read_start_address = 0;
}

static INT16 m_xoffs, m_yoffs;	/* renderer offsets */

void model2_horizontal_sync_w( UINT16 data )
{
	m_crtc_xoffset = 84 + (INT16)data;
	m_xoffs = m_crtc_xoffset;
}

void model2_vertical_sync_w( UINT16 data )
{
	m_crtc_yoffset = 130 + (INT16)data;
	m_yoffs = m_crtc_yoffset;
}



/*************************************
 *  Geometrizer and Z-sort / clip (model2_v.cpp of current MAME)
 *************************************/

static INLINE void transform_point(poly_vertex *point, float *matrix)
{
	float tx = (point->x * matrix[0]) + (point->y * matrix[3]) + (point->pz * matrix[6]) + (matrix[9]);
	float ty = (point->x * matrix[1]) + (point->y * matrix[4]) + (point->pz * matrix[7]) + (matrix[10]);
	float tz = (point->x * matrix[2]) + (point->y * matrix[5]) + (point->pz * matrix[8]) + (matrix[11]);

	point->x = tx;
	point->y = ty;
	point->pz = tz;
}

/* Top Skater (compact object data): a vertex whose x has the lowest bit set
   goes through the second matrix (skinned joints), as in MAME pull
   request #16444 (jkind73, open) */
static INLINE float *geo_vertex_matrix(geo_state *geo, const poly_vertex *p)
{
	return ( geo->compact && ( f2u( p->x ) & 1 ) ) ? geo->matrix2 : geo->matrix;
}

static INLINE void transform_vector(poly_vertex *vector, float *matrix)
{
	float tx = (vector->x * matrix[0]) + (vector->y * matrix[3]) + (vector->pz * matrix[6]);
	float ty = (vector->x * matrix[1]) + (vector->y * matrix[4]) + (vector->pz * matrix[7]);
	float tz = (vector->x * matrix[2]) + (vector->y * matrix[5]) + (vector->pz * matrix[8]);

	vector->x = tx;
	vector->y = ty;
	vector->pz = tz;
}

static INLINE void normalize_vector(poly_vertex *vector)
{
	const float n = sqrt((vector->x * vector->x) + (vector->y * vector->y) + (vector->pz * vector->pz));

	if (n)
	{
		float oon = 1.0f / n;
		vector->x *= oon;
		vector->y *= oon;
		vector->pz *= oon;
	}
}

/* NaN test on the bits: isnan() is C99 (not in MSVC before 2013), and
   x != x can be folded away with -ffast-math */
static INLINE int m2_isnan(float x)
{
	union { float f; UINT32 u; } v;
	v.f = x;
	return (v.u & 0x7f800000) == 0x7f800000 && (v.u & 0x007fffff) != 0;
}

static INLINE float dot_product(poly_vertex v1, poly_vertex v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.pz * v2.pz);
}

static INLINE void vector_cross3(poly_vertex *dst, const poly_vertex *v0, const poly_vertex *v1, const poly_vertex *v2)
{
	poly_vertex p1, p2;

	p1.x = v1->x - v0->x;   p1.y = v1->y - v0->y;   p1.pz = v1->pz - v0->pz;
	p2.x = v2->x - v0->x;   p2.y = v2->y - v0->y;   p2.pz = v2->pz - v0->pz;

	dst->x = (p1.y * p2.pz) - (p1.pz * p2.y);
	dst->y = (p1.pz * p2.x) - (p1.x * p2.pz);
	dst->pz = (p1.x * p2.y) - (p1.y * p2.x);
}

static INLINE void apply_focus(geo_state *geo, poly_vertex *p0)
{
	p0->x *= geo->focus.x;
	p0->y *= geo->focus.y;
}

/* 1.8.23 float to 4.12 float converter, courtesy of Aaron Giles */
static UINT16 float_to_zval(float floatval, INT32 z_adjust)
{
	INT32 fpint = f2u(floatval);
	INT32 exponent = ((fpint >> 23) & 0xff) - ((z_adjust >> 23) & 0xff);
	UINT32 mantissa = fpint & 0x7fffff;

	/* round the low bits and reduce to 12 */
	mantissa += 0x400;
	if (mantissa > 0x7fffff)
	{
		exponent++;
		mantissa = (mantissa & 0x7fffff) >> 1;
	}
	mantissa >>= 11;

	/* if negative, clamp to 0 */
	if (fpint < 0)
		return 0x0000;

	/* the rest depends on the exponent */
	if (exponent < -12)
		return 0x0000; /* less than -12 is too small, return 0 */
	else if (exponent < 0)
		return (mantissa | 0x1000) >> -exponent; /* between -12 and 0 create a denormal with exponent of 0 */
	else if (exponent < 15)
		return ((exponent + 1) << 12) | mantissa; /* between 0 and 14 create a FP value with exponent + 1 */
	else
		return 0xffff; /* above 14 is too large */
}

static INT32 clip_polygon(poly_vertex *v, INT32 num_vertices, poly_vertex *vout, plane clip_plane)
{
	INT32 outcount = 0;

	const poly_vertex *cur = v;
	poly_vertex *out = vout;

	float curdot = dot_product(*cur, clip_plane.normal);
	INT32 curin = (curdot >= clip_plane.distance) ? 1 : 0;
	INT32 i;

	for (i = 0; i < num_vertices; i++)
	{
		const INT32 nextvert = (i + 1) % num_vertices;
		float nextdot;
		INT32 nextin;

		/* if the current point is inside the plane, add it */
		if (curin)
			out[outcount++] = *cur;

		nextdot = dot_product(v[nextvert], clip_plane.normal);
		nextin = (nextdot >= clip_plane.distance) ? 1 : 0;

		/* Add a clipped vertex if one end of the current edge is inside the plane and the other is outside */
		if ((curin != nextin) && !m2_isnan(curdot) && !m2_isnan(nextdot))
		{
			const float scale = (clip_plane.distance - curdot) / (nextdot - curdot);

			out[outcount].x = cur->x + ((v[nextvert].x - cur->x) * scale);
			out[outcount].y = cur->y + ((v[nextvert].y - cur->y) * scale);
			out[outcount].pz = cur->pz + ((v[nextvert].pz - cur->pz) * scale);
			out[outcount].pu = cur->pu + ((v[nextvert].pu - cur->pu) * scale);
			out[outcount].pv = cur->pv + ((v[nextvert].pv - cur->pv) * scale);
			outcount++;
		}

		curdot = nextdot;
		curin = nextin;
		cur++;
	}

	return outcount;
}

static int check_culling(raster_state *raster, UINT32 attr, float min_z, float max_z)
{
	/* if doubleside is disabled */
	if (((attr >> 17) & 1) == 0)
	{
		/* if it's the backface, cull it */
		if (raster->command_buffer[9] & 0x00800000)
			return 1;
	}

	/* if the linktype is 0, then we can also cull it */
	if (((attr >> 8) & 3) == 0)
		return 1;

	/* if the minimum z value is bigger than the master z clip value, don't render */
	if (raster->master_z_clip != 0xff && (INT32)(1.0 / min_z) > raster->master_z_clip)
		return 1;

	/* if the maximum z value is < 0 then we can safely clip the entire polygon */
	if (max_z < 0)
		return 1;

	return 0;
}


static void model2_3d_process_polygon(raster_state *raster, UINT32 attr, unsigned NumVerts)
{
	quad_m2 object;
	UINT16 *th, *tp;
	INT32 tho;
	UINT32 i;
	int cull;
	float zvalue;
	float min_z, max_z;


	/* extract P0(n-1) */
	object.v[1].x = u2f(raster->command_buffer[2] << 8);
	object.v[1].y = u2f(raster->command_buffer[3] << 8);
	object.v[1].pz = u2f(raster->command_buffer[4] << 8);

	/* extract P1(n-1) */
	object.v[0].x = u2f(raster->command_buffer[5] << 8);
	object.v[0].y = u2f(raster->command_buffer[6] << 8);
	object.v[0].pz = u2f(raster->command_buffer[7] << 8);

	/* extract P0(n) */
	object.v[2].x = u2f(raster->command_buffer[11] << 8);
	object.v[2].y = u2f(raster->command_buffer[12] << 8);
	object.v[2].pz = u2f(raster->command_buffer[13] << 8);

	if (NumVerts == 4)
	{
		/* extract P1(n) */
		object.v[3].x = u2f(raster->command_buffer[14] << 8);
		object.v[3].y = u2f(raster->command_buffer[15] << 8);
		object.v[3].pz = u2f(raster->command_buffer[16] << 8);
	}
	else
	{
		/* for triangles, the rope of P1(n) is achieved by P0(n-1) (linktype 3) */
		raster->command_buffer[14] = raster->command_buffer[11];
		raster->command_buffer[15] = raster->command_buffer[12];
		raster->command_buffer[16] = raster->command_buffer[13];
	}

	/* always calculate the min z and max z value */
	min_z = object.v[0].pz;
	if (object.v[1].pz < min_z) min_z = object.v[1].pz;
	if (object.v[2].pz < min_z) min_z = object.v[2].pz;
	if (NumVerts == 4 && object.v[3].pz < min_z) min_z = object.v[3].pz;

	max_z = object.v[0].pz;
	if (object.v[1].pz > max_z) max_z = object.v[1].pz;
	if (object.v[2].pz > max_z) max_z = object.v[2].pz;
	if (NumVerts == 4 && object.v[3].pz > max_z) max_z = object.v[3].pz;

	/* read in the texture information */

	/* texture point data */
	if (raster->command_buffer[0] & 0x800000)
		tp = &raster->texture_ram[raster->command_buffer[0] & 0xffff];
	else
		tp = &raster->texture_rom[raster->command_buffer[0] & raster->texture_rom_mask];

	object.v[0].pv = *tp++;
	object.v[0].pu = *tp++;
	object.v[1].pv = *tp++;
	object.v[1].pu = *tp++;
	object.v[2].pv = *tp++;
	object.v[2].pu = *tp++;
	if (NumVerts == 4)
	{
		object.v[3].pv = *tp++;
		object.v[3].pu = *tp++;
	}

	/* update the address */
	raster->command_buffer[0] += NumVerts * 2;

	/* texture header data */
	if (raster->command_buffer[1] & 0x800000)
		th = &raster->texture_ram[raster->command_buffer[1] & 0xffff];
	else
		th = &raster->texture_rom[raster->command_buffer[1] & raster->texture_rom_mask];

	object.texheader[0] = *th++;
	object.texheader[1] = *th++;
	object.texheader[2] = *th++;
	object.texheader[3] = *th++;

	/* extract the texture header offset */
	tho = (attr >> 12) & 0x1f;

	/* adjust for sign */
	if (tho & 0x10)
		tho |= -16;

	/* update the address */
	raster->command_buffer[1] += tho * 4;

	/* set the luma value of this polygon */
	object.luma = (raster->command_buffer[9] >> 15) & 0xff;

	/* set the texture LOD of this polygon */
	object.texlod = ((raster->command_buffer[10] >> 8) & 0x7f80) - 0x3f80;
	object.texlod += raster->log_ram[raster->command_buffer[10] & 0x7fff];

	/* determine whether we can cull this polygon */
	cull = check_culling(raster,attr,min_z,max_z);

	/* set the object's z value */
	switch ((attr >> 10) & 3)
	{
		case 0: /* old value */
			zvalue = raster->polygon_z;
			break;
		case 1: /* min z */
			zvalue = min_z;
			break;
		case 2: /* max z */
			zvalue = max_z;
			break;
		case 3: /* error */
		default:
			zvalue = 1e10;
			break;
	}

	raster->polygon_z = zvalue;

	if (cull == 0)
	{
		INT32 clipped_verts;
		/* a convex polygon gains at most one vertex for each of the four
		   planes, but a bad one (self-crossing or with NaN coordinates, as
		   Top Skater sends) can double them on each plane: room for that,
		   and only polygons that still fit in 8 vertices are kept (MAME's
		   8-vertex buffers overflow on these) */
		poly_vertex verts_in[64], verts_out[64];
		int j;

		for (i = 0; i < NumVerts; i++)
			verts_in[i] = object.v[i];

		clipped_verts = NumVerts;

		/* do clipping */
		for (i = 0; i < 4; i++)
		{
			clipped_verts = clip_polygon(verts_in, clipped_verts, verts_out, raster->clip_plane[raster->center_sel][i]);
			for (j = 0; j < clipped_verts; j++)
				verts_in[j] = verts_out[j];
		}

		if (clipped_verts > 2 && clipped_verts <= 8)
		{
			polygon *zpoly;
			polygon *poly;

			/* adjust and set the object z-sort value */
			object.z = float_to_zval(zvalue, raster->z_adjust);

			/* get our list read to add the polygons */
			zpoly = raster->poly_sorted_list[object.z];

			/* go through the clipped vertex list, adding polygons */
			if (raster->poly_list_index >= MAX_POLYGONS - 1)
			{
				m2_error("SEGA 3D: Max polygon limit exceeded\n");
				return;
			}
			poly = &raster->poly_list[raster->poly_list_index++];

			/* copy the object information */
			poly->z = object.z;
			poly->texheader[0] = object.texheader[0];
			poly->texheader[1] = object.texheader[1];
			poly->texheader[2] = object.texheader[2];
			poly->texheader[3] = object.texheader[3];
			poly->luma = object.luma;
			poly->texlod = object.texlod;

			/* set the viewport */
			poly->viewport[0] = raster->viewport[0];
			poly->viewport[1] = raster->viewport[1];
			poly->viewport[2] = raster->viewport[2];
			poly->viewport[3] = raster->viewport[3];

			/* set the center */
			poly->center[0] = raster->center[raster->center_sel][0];
			poly->center[1] = raster->center[raster->center_sel][1];

			/* set the window */
			poly->window = raster->cur_window;

			poly->num_vertices = clipped_verts;

			for (i = 0; i < clipped_verts; i++)
				poly->v[i] = verts_out[i];

			/* add to our sorted list */
			raster->poly_sorted_list[object.z] = poly;
			poly->next = zpoly;
			zpoly = poly;

			/* keep around the min and max z values for this frame */
			if (object.z < raster->min_z) raster->min_z = object.z;
			if (object.z > raster->max_z) raster->max_z = object.z;
		}
	}

	/* update linking */
	switch (((attr >> 8) & 3))
	{
		case 0:
		case 2:
		{
			/* reuse P0(n) and P1(n) */
			for (i = 0; i < 6; i++)                                        /* P0(n) -> P0(n-1) */
				raster->command_buffer[2+i] = raster->command_buffer[11+i]; /* P1(n) -> P1(n-1) */
		}
		break;

		case 1:
		{
			/* reuse P0(n-1) and P0(n) */
			for (i = 0; i < 3; i++)
				raster->command_buffer[5+i] = raster->command_buffer[11+i]; /* P0(n) -> P1(n-1) */
		}
		break;

		case 3:
		{
			/* reuse P1(n-1) and P1(n) */
			for (i = 0; i < 3; i++)
				raster->command_buffer[2+i] = raster->command_buffer[14+i]; /* P1(n) -> P1(n-1) */
		}
		break;
	}
}


static void model2_3d_push(raster_state *raster, UINT32 input)
{
	/* see if we have a command in progress */
	if (raster->cur_command != 0)
	{
		raster->command_buffer[raster->command_index++] = input;

		switch (raster->cur_command)
		{
			case 0x00:  /* NOP */
			break;

			case 0x01:  /* Polygon Data */
			{
				UINT32  attr;

				/* start by looking if we have the basic input data */
				if (raster->command_index < 9)
					return;

				/* get the attributes */
				attr = raster->command_buffer[8];

				/* see if we're done */
				if ((attr & 3) == 0)
				{
					raster->cur_command = 0;
					return;
				}

				/* see if it's a quad or a triangle */
				if (attr & 1)
				{
					/* it's a quad, wait for the rest of the points */
					if (raster->command_index < 17)
						return;

					/* we have a full polygon info, fill up our polygon structure */
					model2_3d_process_polygon(raster, attr, 4);

					/* back up and wait for more data */
					raster->command_index = 8;
				}
				else
				{
					/* it's a triangle, wait for the rest of the point */
					if (raster->command_index < 14)
						return;

					/* we have a full polygon info, fill up our polygon structure */
					model2_3d_process_polygon(raster, attr, 3);

					/* back up and wait for more data */
					raster->command_index = 8;
				}
			}
			break;

			case 0x03:  /* Window Data */
			{
				UINT32  i;

				/* make sure we have all the data */
				if (raster->command_index < 6)
					return;

				/* coordinates are 12 bit signed */

				/* extract the viewport start x */
				raster->viewport[0] = (raster->command_buffer[0] >> 12) & 0xfff;

				if (raster->viewport[0] & 0x800)
					raster->viewport[0] = -(0x800 - (raster->viewport[0] & 0x7ff));

				/* extract the viewport start y */
				raster->viewport[1] = raster->command_buffer[0] & 0xfff;

				if (raster->viewport[1] & 0x800)
					raster->viewport[1] = -(0x800 - (raster->viewport[1] & 0x7ff));

				/* extract the viewport end x */
				raster->viewport[2] = (raster->command_buffer[1] >> 12) & 0xfff;

				if (raster->viewport[2] & 0x800)
					raster->viewport[2] = -(0x800 - (raster->viewport[2] & 0x7ff));

				/* extract the viewport end y */
				raster->viewport[3] = raster->command_buffer[1] & 0xfff;

				if (raster->viewport[3] & 0x800)
					raster->viewport[3] = -(0x800 - (raster->viewport[3] & 0x7ff));

				/* extract the centers */
				for (i = 0; i < 4; i++)
				{
					/* center x */
					raster->center[i][0] = (raster->command_buffer[2+i] >> 12) & 0xfff;

					if (raster->center[i][0] & 0x800)
						raster->center[i][0] = -(0x800 - (raster->center[i][0] & 0x7ff));

					/* center y */
					raster->center[i][1] = raster->command_buffer[2+i] & 0xfff;

					if (raster->center[i][1] & 0x800)
						raster->center[i][1] = -(0x800 - (raster->center[i][1] & 0x7ff));

					{
					/* calculate clipping planes */
					float left_plane = (float)(raster->center[i][0] - raster->viewport[0]);
					float right_plane = (float)(raster->viewport[2] - raster->center[i][0]);
					float top_plane = (float)(raster->viewport[3] - raster->center[i][1]);
					float bottom_plane = (float)(raster->center[i][1] - raster->viewport[1]);

					raster->clip_plane[i][0].normal.x = 1.0f / hypot(1.0f, left_plane);
					raster->clip_plane[i][0].normal.y = 0.0f;
					raster->clip_plane[i][0].normal.pz = left_plane / hypot(1.0f, left_plane);

					raster->clip_plane[i][1].normal.x = -1.0f / hypot(-1.0f, right_plane);
					raster->clip_plane[i][1].normal.y = 0.0f;
					raster->clip_plane[i][1].normal.pz = right_plane / hypot(-1.0f, right_plane);

					raster->clip_plane[i][2].normal.x = 0.0f;
					raster->clip_plane[i][2].normal.y = -1.0f / hypot(-1.0f, top_plane);
					raster->clip_plane[i][2].normal.pz = top_plane / hypot(-1.0f, top_plane);

					raster->clip_plane[i][3].normal.x = 0.0f;
					raster->clip_plane[i][3].normal.y = 1.0f / hypot(1.0f, bottom_plane);
					raster->clip_plane[i][3].normal.pz = bottom_plane / hypot(1.0f, bottom_plane);
					}
				}

				/* done with this command */
				raster->cur_command = 0;
			}
			break;

			case 0x04:  /* Texture/Log Data write */
			{
				/* make sure we have enough data */
				if (raster->command_index < 2)
					return;

				/* see if the count is non-zero */
				if (raster->command_buffer[1] > 0)
				{
					/* see if we have data available */
					if (raster->command_index >= 3)
					{
						/* get the address */
						UINT32  address = raster->command_buffer[0];

						/* do the write */
						if (address & 0x800000)
							raster->texture_ram[address & 0xffff] = raster->command_buffer[2];
						else
							raster->log_ram[address & 0x7fff] = raster->command_buffer[2];

						/* increment the address and decrease the count */
						raster->command_buffer[0]++;
						raster->command_buffer[1]--;

						/* decrease the index, so we keep placing data in the same slot */
						raster->command_index--;
					}
				}

				/* see if we're done with this command */
				if (raster->command_buffer[1] == 0)
					raster->cur_command = 0;
			}
			break;

			case 0x08:  /* ZSort mode */
			{
				/* save the zsort mode value */
				raster->z_adjust = raster->command_buffer[0] << 8;

				/* done with this command */
				raster->cur_command = 0;
			}
			break;

			default:
			{
				m2_error("SEGA 3D: Unknown rasterizer command\n");
			}
		}
	}
	else
	{
		/* new command */
		raster->cur_command = input & 0x0f;
		raster->command_index = 0;

		/* see if it's object data */
		if (raster->cur_command == 1)
		{
			/* extract reverse bit */
			raster->reverse = (input >> 4) & 1;

			/* extract center select */
			raster->center_sel = (input >> 6) & 3;
		}
	}
}


/* Parse Polygons: Normals Present, No Specular case */
static void geo_parse_np_ns(geo_state *geo, UINT32 *input, UINT32 count)
{
	raster_state *raster = geo->raster;
	poly_vertex point, normal;
	UINT32  attr, i;

	/* read the 1st point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo->matrix);

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* read the 2nd point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo->matrix);

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* loop through the following links */
	for (i = 0; i < count; i++)
	{
		/* read in the attributes */
		attr = *input++;

		/* push to the 3d rasterizer */
		model2_3d_push(raster, attr & 0x0003ffff);

		/* read in the normal */
		normal.x = u2f(*input++);
		normal.y = u2f(*input++);
		normal.pz = u2f(*input++);

		/* transform with the current matrix */
		transform_vector(&normal, geo->matrix);

		if ((attr & 3) != 0) /* quad or triangle */
		{
			float               dotl, dotp, luminance, distance;
			float               coef, face;
			INT32               luma;
			texture_parameter * texparam;

			/* read in the next point */
			point.x = u2f(*input++);
			point.y = u2f(*input++);
			point.pz = u2f(*input++);

			/* transform with the current matrix */
			transform_point(&point, geo->matrix);

			/* calculate the dot product of the normal and the light vector */
			dotl = dot_product(normal, geo->light);

			/* calculate the dot product of the normal and the point */
			dotp = dot_product(normal, point);

			/* apply focus */
			apply_focus(geo, &point);

			/* determine whether this is the front or the back of the polygon */
			face = 0x100; /* rear */
			if (dotp >= 0) face = 0; /* front */

			/* get the texture parameters */
			texparam = &geo->texture_parameters[(attr>>18) & 0x1f];

			/* calculate luminance */
			if ((dotl * dotp) < 0) luminance = 0;
			else luminance = fabs(dotl);

			luminance = (luminance * texparam->diffuse) + texparam->ambient;
			luminance = CLAMPF(luminance, 0.0f, 255.0f);

			luma = (INT32)luminance;

			/* add the face bit to the luma */
			luma += face;

			/* extract distance coefficient */
			coef = geo->coef_table[attr>>27];

			/* calculate texture level of detail */
			distance = coef * fabs(dotp) * geo->lod;

			/* push to the 3d rasterizer */
			model2_3d_push(raster, luma << 15);
			model2_3d_push(raster, f2u(distance) >> 8);
			model2_3d_push(raster, f2u(point.x) >> 8);
			model2_3d_push(raster, f2u(point.y) >> 8);
			model2_3d_push(raster, f2u(point.pz) >> 8);

			/* if it's a quad, push one more point */
			if (attr & 1)
			{
				/* read in the next point */
				point.x = u2f(*input++);
				point.y = u2f(*input++);
				point.pz = u2f(*input++);

				/* transform with the current matrix */
				transform_point(&point, geo->matrix);

				/* apply focus */
				apply_focus(geo, &point);

				/* push to the 3d rasterizer */
				model2_3d_push(raster, f2u(point.x) >> 8);
				model2_3d_push(raster, f2u(point.y) >> 8);
				model2_3d_push(raster, f2u(point.pz) >> 8);
			}
			else /* triangle */
			{
				/* skip the next 3 points */
				input += 3;
			}
		}
		else /* we're done */
		{
			break;
		}
	}

	/* notify the 3d rasterizer we're done */
	model2_3d_push(raster, 0);
}

/* Parse Polygons: Normals Present, Specular case */
static void geo_parse_np_s(geo_state *geo, UINT32 *input, UINT32 count)
{
	raster_state *raster = geo->raster;
	poly_vertex point, normal;
	UINT32  attr, i;

	/* read the 1st point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo->matrix);

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* read the 2nd point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo->matrix);

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* loop through the following links */
	for (i = 0; i < count; i++)
	{
		/* read in the attributes */
		attr = *input++;

		/* push to the 3d rasterizer */
		model2_3d_push(raster, attr & 0x0003ffff);

		/* read in the normal */
		normal.x = u2f(*input++);
		normal.y = u2f(*input++);
		normal.pz = u2f(*input++);

		/* transform with the current matrix */
		transform_vector(&normal, geo->matrix);

		if ((attr & 3) != 0) /* quad or triangle */
		{
			float               dotl, dotp, luminance, distance, specular;
			float               coef, face;
			INT32             luma;
			texture_parameter * texparam;

			/* read in the next point */
			point.x = u2f(*input++);
			point.y = u2f(*input++);
			point.pz = u2f(*input++);

			/* transform with the current matrix */
			transform_point(&point, geo->matrix);

			/* calculate the dot product of the normal and the light vector */
			dotl = dot_product(normal, geo->light);

			/* calculate the dot product of the normal and the point */
			dotp = dot_product(normal, point);

			/* apply focus */
			apply_focus(geo, &point);

			/* determine whether this is the front or the back of the polygon */
			face = 0x100; /* rear */
			if (dotp >= 0) face = 0; /* front */

			/* get the texture parameters */
			texparam = &geo->texture_parameters[(attr>>18) & 0x1f];

			/* calculate luminance and specular */
			if ((dotl * dotp) < 0) luminance = 0;
			else luminance = fabs(dotl);

			specular = ((2*dotl) * normal.pz) - geo->light.pz;
			if (specular < 0) specular = 0;
			if (texparam->specular_control == 0) specular = 0;
			if ((texparam->specular_control >> 1) != 0) specular *= specular;
			if ((texparam->specular_control >> 2) != 0) specular *= specular;
			if (((texparam->specular_control+1) >> 3) != 0) specular *= specular;

			specular *= texparam->specular_scale;

			luminance = (luminance * texparam->diffuse) + texparam->ambient + specular;
			luminance = CLAMPF(luminance, 0.0f, 255.0f);

			luma = (INT32)luminance;

			/* add the face bit to the luma */
			luma += face;

			/* extract distance coefficient */
			coef = geo->coef_table[attr>>27];

			/* calculate texture level of detail */
			distance = coef * fabs(dotp) * geo->lod;

			/* push to the 3d rasterizer */
			model2_3d_push(raster, luma << 15);
			model2_3d_push(raster, f2u(distance) >> 8);
			model2_3d_push(raster, f2u(point.x) >> 8);
			model2_3d_push(raster, f2u(point.y) >> 8);
			model2_3d_push(raster, f2u(point.pz) >> 8);

			/* if it's a quad, push one more point */
			if (attr & 1)
			{
				/* read in the next point */
				point.x = u2f(*input++);
				point.y = u2f(*input++);
				point.pz = u2f(*input++);

				/* transform with the current matrix */
				transform_point(&point, geo->matrix);

				/* apply focus */
				apply_focus(geo, &point);

				/* push to the 3d rasterizer */
				model2_3d_push(raster, f2u(point.x) >> 8);
				model2_3d_push(raster, f2u(point.y) >> 8);
				model2_3d_push(raster, f2u(point.pz) >> 8);
			}
			else /* triangle */
			{
				/* skip the next 3 points */
				input += 3;
			}
		}
		else /* we're done */
		{
			break;
		}
	}

	/* notify the 3d rasterizer we're done */
	model2_3d_push(raster, 0);
}

/* Parse Polygons: No Normals, No Specular case */
static void geo_parse_nn_ns(geo_state *geo, UINT32 *input, UINT32 count)
{
	raster_state *raster = geo->raster;
	poly_vertex point, normal, p0, p1, p2, p3;
	UINT32  attr, i;

	/* read the 1st point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo_vertex_matrix(geo, &point));

	/* save for normal calculation */
	p0.x = point.x; p0.y = point.y; p0.pz = point.pz;

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* read the 2nd point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo_vertex_matrix(geo, &point));

	/* save for normal calculation */
	p1.x = point.x; p1.y = point.y; p1.pz = point.pz;

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* loop through the following links */
	for (i = 0; i < count; i++)
	{
		/* read in the attributes */
		attr = *input++;

		/* push to the 3d rasterizer */
		model2_3d_push(raster, attr & 0x0003ffff);

		if ((attr & 3) != 0) /* quad or triangle */
		{
			float               dotl, dotp, luminance, distance;
			float               coef, face;
			INT32             luma;
			texture_parameter * texparam;

			/* Skip normal */
			if (!geo->compact) input += 3;

			/* read in the next point */
			point.x = u2f(*input++);
			point.y = u2f(*input++);
			point.pz = u2f(*input++);

			/* transform with the current matrix */
			transform_point(&point, geo_vertex_matrix(geo, &point));

			/* save for normal calculation */
			p2.x = point.x; p2.y = point.y; p2.pz = point.pz;

			/* compute the normal */
			vector_cross3(&normal, &p0, &p1, &p2);

			/* normalize it */
			normalize_vector(&normal);

			/* calculate the dot product of the normal and the light vector */
			dotl = dot_product(normal, geo->light);

			/* calculate the dot product of the normal and the point */
			dotp = dot_product(normal, point);

			/* apply focus */
			apply_focus(geo, &point);

			/* determine whether this is the front or the back of the polygon */
			face = 0x100; /* rear */
			if (dotp >= 0) face = 0; /* front */

			/* get the texture parameters */
			texparam = &geo->texture_parameters[(attr>>18) & 0x1f];

			/* calculate luminance */
			if ((dotl * dotp) < 0) luminance = 0;
			else luminance = fabs(dotl);

			luminance = (luminance * texparam->diffuse) + texparam->ambient;
			luminance = CLAMPF(luminance, 0.0f, 255.0f);

			luma = (INT32)luminance;

			/* add the face bit to the luma */
			luma += face;

			/* extract distance coefficient */
			coef = geo->coef_table[attr>>27];

			/* calculate texture level of detail */
			distance = coef * fabs(dotp) * geo->lod;

			/* push to the 3d rasterizer */
			model2_3d_push(raster, luma << 15);
			model2_3d_push(raster, f2u(distance) >> 8);
			model2_3d_push(raster, f2u(point.x) >> 8);
			model2_3d_push(raster, f2u(point.y) >> 8);
			model2_3d_push(raster, f2u(point.pz) >> 8);

			/* if it's a quad, push one more point */
			if (attr & 1)
			{
				/* read in the next point */
				point.x = u2f(*input++);
				point.y = u2f(*input++);
				point.pz = u2f(*input++);

				/* transform with the current matrix */
				transform_point(&point, geo_vertex_matrix(geo, &point));

				/* save for normal calculation */
				p3.x = point.x; p3.y = point.y; p3.pz = point.pz;

				/* apply focus */
				apply_focus(geo, &point);

				/* push to the 3d rasterizer */
				model2_3d_push(raster, f2u(point.x) >> 8);
				model2_3d_push(raster, f2u(point.y) >> 8);
				model2_3d_push(raster, f2u(point.pz) >> 8);
			}
			else
			{
				/* skip the next 3 points */
				if (!geo->compact) input += 3;

				/* for triangles, the rope of P1(n) is achieved by P0(n-1) (linktype 3) */
				p3.x = p2.x; p3.y = p2.y; p3.pz = p2.pz;
			}
		}
		else /* we're done */
		{
			break;
		}

		/* link type */
		switch ((attr>>8) & 3)
		{
			case 0:
			case 2:
			{
				/* reuse P0(n) and P1(n) */
				p0.x = p2.x; p0.y = p2.y; p0.pz = p2.pz;
				p1.x = p3.x; p1.y = p3.y; p1.pz = p3.pz;
			}
			break;

			case 1:
			{
				/* reuse P0(n-1) and P0(n) */
				p1.x = p2.x; p1.y = p2.y; p1.pz = p2.pz;
			}
			break;

			case 3:
			{
				/* reuse P1(n-1) and P1(n) */
				p0.x = p3.x; p0.y = p3.y; p0.pz = p3.pz;
			}
			break;
		}
	}

	/* notify the 3d rasterizer we're done */
	model2_3d_push(raster, 0);
}

/* Parse Polygons: No Normals, Specular case */
static void geo_parse_nn_s(geo_state *geo, UINT32 *input, UINT32 count)
{
	raster_state *raster = geo->raster;
	poly_vertex point, normal, p0, p1, p2, p3;
	UINT32  attr, i;

	/* read the 1st point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo_vertex_matrix(geo, &point));

	/* save for normal calculation */
	p0.x = point.x; p0.y = point.y; p0.pz = point.pz;

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* read the 2nd point */
	point.x = u2f(*input++);
	point.y = u2f(*input++);
	point.pz = u2f(*input++);

	/* transform with the current matrix */
	transform_point(&point, geo_vertex_matrix(geo, &point));

	/* save for normal calculation */
	p1.x = point.x; p1.y = point.y; p1.pz = point.pz;

	/* apply focus */
	apply_focus(geo, &point);

	/* push it to the 3d rasterizer */
	model2_3d_push(raster, f2u(point.x) >> 8);
	model2_3d_push(raster, f2u(point.y) >> 8);
	model2_3d_push(raster, f2u(point.pz) >> 8);

	/* loop through the following links */
	for (i = 0; i < count; i++)
	{
		/* read in the attributes */
		attr = *input++;

		/* push to the 3d rasterizer */
		model2_3d_push(raster, attr & 0x0003ffff);

		if ((attr & 3) != 0) /* quad or triangle */
		{
			float               dotl, dotp, luminance, distance, specular;
			float               coef, face;
			INT32             luma;
			texture_parameter * texparam;

			/* Skip normal */
			if (!geo->compact) input += 3;

			/* read in the next point */
			point.x = u2f(*input++);
			point.y = u2f(*input++);
			point.pz = u2f(*input++);

			/* transform with the current matrix */
			transform_point(&point, geo_vertex_matrix(geo, &point));

			/* save for normal calculation */
			p2.x = point.x; p2.y = point.y; p2.pz = point.pz;

			/* compute the normal */
			vector_cross3(&normal, &p0, &p1, &p2);

			/* normalize it */
			normalize_vector(&normal);

			/* calculate the dot product of the normal and the light vector */
			dotl = dot_product(normal, geo->light);

			/* calculate the dot product of the normal and the point */
			dotp = dot_product(normal, point);

			/* apply focus */
			apply_focus(geo, &point);

			/* determine whether this is the front or the back of the polygon */
			face = 0x100; /* rear */
			if (dotp >= 0) face = 0; /* front */

			/* get the texture parameters */
			texparam = &geo->texture_parameters[(attr>>18) & 0x1f];

			/* calculate luminance and specular */
			if ((dotl * dotp) < 0) luminance = 0;
			else luminance = fabs(dotl);

			specular = ((2*dotl) * normal.pz) - geo->light.pz;
			if (specular < 0) specular = 0;
			if (texparam->specular_control == 0) specular = 0;
			if ((texparam->specular_control >> 1) != 0) specular *= specular;
			if ((texparam->specular_control >> 2) != 0) specular *= specular;
			if (((texparam->specular_control+1) >> 3) != 0) specular *= specular;

			specular *= texparam->specular_scale;

			luminance = (luminance * texparam->diffuse) + texparam->ambient + specular;
			luminance = CLAMPF(luminance, 0.0f, 255.0f);

			luma = (INT32)luminance;

			/* add the face bit to the luma */
			luma += face;

			/* extract distance coefficient */
			coef = geo->coef_table[attr>>27];

			/* calculate texture level of detail */
			distance = coef * fabs(dotp) * geo->lod;

			/* push to the 3d rasterizer */
			model2_3d_push(raster, luma << 15);
			model2_3d_push(raster, f2u(distance) >> 8);
			model2_3d_push(raster, f2u(point.x) >> 8);
			model2_3d_push(raster, f2u(point.y) >> 8);
			model2_3d_push(raster, f2u(point.pz) >> 8);

			/* if it's a quad, push one more point */
			if (attr & 1)
			{
				/* read in the next point */
				point.x = u2f(*input++);
				point.y = u2f(*input++);
				point.pz = u2f(*input++);

				/* transform with the current matrix */
				transform_point(&point, geo_vertex_matrix(geo, &point));

				/* save for normal calculation */
				p3.x = point.x; p3.y = point.y; p3.pz = point.pz;

				/* apply focus */
				apply_focus(geo, &point);

				/* push to the 3d rasterizer */
				model2_3d_push(raster, f2u(point.x) >> 8);
				model2_3d_push(raster, f2u(point.y) >> 8);
				model2_3d_push(raster, f2u(point.pz) >> 8);
			}
			else
			{
				/* skip the next 3 points */
				if (!geo->compact) input += 3;

				/* for triangles, the rope of P1(n) is achieved by P0(n-1) (linktype 3) */
				p3.x = p2.x; p3.y = p2.y; p3.pz = p2.pz;
			}
		}
		else /* we're done */
		{
			break;
		}

		/* link type */
		switch ((attr>>8) & 3)
		{
			case 0:
			case 2:
			{
				/* reuse P0(n) and P1(n) */
				p0.x = p2.x; p0.y = p2.y; p0.pz = p2.pz;
				p1.x = p3.x; p1.y = p3.y; p1.pz = p3.pz;
			}
			break;

			case 1:
			{
				/* reuse P0(n-1) and P0(n) */
				p1.x = p2.x; p1.y = p2.y; p1.pz = p2.pz;
			}
			break;

			case 3:
			{
				/* reuse P1(n-1) and P1(n) */
				p0.x = p3.x; p0.y = p3.y; p0.pz = p3.pz;
			}
			break;
		}
	}

	/* notify the 3d rasterizer we're done */
	model2_3d_push(raster, 0);
}

/*******************************************
 *
 *  Geometry Engine Commands
 *
 *******************************************/

/* Command 00: NOP */
static UINT32 *geo_nop(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;

	/* push the opcode to the 3d rasterizer */
	model2_3d_push(raster, opcode >> 23);

	return input;
}

/* Command 01: Object Data */
static UINT32 *geo_object_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;
	UINT32  tpa = *input++;     /* Texture Point Address */
	UINT32  tha = *input++;     /* Texture Header Address */
	UINT32  oba = *input++;     /* Object Address */
	UINT32  obc = *input++;     /* Object Count */

	UINT32 *obp;                /* Object Pointer */

	/* push the initial set of data to the 3d rasterizer (Top Skater's
	   program also passes the center select, bits 6-7 of the command) */
	model2_3d_push(raster, (opcode >> 23) | (geo->compact ? (opcode & 0xc0) : 0));
	model2_3d_push(raster, tpa);
	model2_3d_push(raster, tha);

	/* select where we're reading polygon information from */
	if (oba & 0x01000000)
	{
		/* Fast polygon RAM */
		obp = &geo->polygon_ram1[oba & 0x7fff];
	}
	else if (oba & 0x00800000)
	{
		/* Polygon ROM */
		obp = &geo->polygon_rom[oba & geo->polygon_rom_mask];
	}
	else
	{
		/* Slow Polygon RAM */
		obp = &geo->polygon_ram0[oba & 0x7fff];
	}

	/* if count == 0 then rolls over to max size */
	/* Virtual On & Gunblade NY */
	if (obc == 0)
		obc = 0xfffff;

	switch (geo->mode & 3)
	{
		/* Normals present, No Specular */
		case 0: geo_parse_np_ns(geo, obp, obc); break;

		/* Normals present, Specular */
		case 1: geo_parse_np_s(geo, obp, obc); break;

		/* No Normals present, No Specular */
		case 2: geo_parse_nn_ns(geo, obp, obc); break;

		/* No Normals present, Specular */
		case 3: geo_parse_nn_s(geo, obp, obc); break;
	}

	/* move by 4 parameters */
	return input;
}

/* Command 02: Direct Data */
static UINT32 *geo_direct_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  attr;
	raster_state *raster = geo->raster;
	UINT32  tpa = *input++;     /* Texture Point Address */
	UINT32  tha = *input++;     /* Texture Header Address */

	/* push the initial set of data to the 3d rasterizer */
	model2_3d_push(raster, (opcode >> 23) - 1);
	model2_3d_push(raster, tpa);
	model2_3d_push(raster, tha);

	/* push the initial points */
	model2_3d_push(raster, (*input++) >> 8); /* x */
	model2_3d_push(raster, (*input++) >> 8); /* y */
	model2_3d_push(raster, (*input++) >> 8); /* z */

	model2_3d_push(raster, (*input++) >> 8); /* x */
	model2_3d_push(raster, (*input++) >> 8); /* y */
	model2_3d_push(raster, (*input++) >> 8); /* z */

	/* read in the attributes */
	while (((attr = *input++) & 3) != 0)
	{
		/* push attributes */
		model2_3d_push(raster, attr & 0x00ffffff);

		/* push luma */
		model2_3d_push(raster, (*input++) >> 8);

		/* push distance */
		model2_3d_push(raster, (*input++) >> 8);

		/* push the next point */
		model2_3d_push(raster, (*input++) >> 8); /* x */
		model2_3d_push(raster, (*input++) >> 8); /* y */
		model2_3d_push(raster, (*input++) >> 8); /* z */

		/* if it's a quad, output another point */
		if (attr & 1)
		{
			model2_3d_push(raster, (*input++) >> 8); /* x */
			model2_3d_push(raster, (*input++) >> 8); /* y */
			model2_3d_push(raster, (*input++) >> 8); /* z */
		}
	}

	/* we're done */
	model2_3d_push(raster, 0);

	return input;
}

/* Command 03: Window Data */
static UINT32 *geo_window_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;
	UINT32 i;

	/* start by pushing the opcode */
	model2_3d_push(raster, opcode >> 23);

	raster->cur_window++;

	/*
	    we're going to move 6 coordinates to the 3d rasterizer:
	    - starting coordinate
	    - completion coordinate
	    - vanishing point 0 (eye mode 0)
	    - vanishing point 1 (eye mode 1)
	    - vanishing point 2 (eye mode 2)
	    - vanishing point 3 (eye mode 3)
	*/

	for (i = 0; i < 6; i++)
	{
		/* read in the coordinate */
		UINT32 y = *input++;

		/* convert to the 3d rasterizer format (00XXXYYY) */
		UINT32 x = (y & 0x0fff0000) >> 4 ;
		y &= 0xfff;

		/* push it */
		model2_3d_push(raster, x | y);
	}

	return input;
}

/* Command 04: Texture Data Write */
static UINT32 *geo_texture_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;
	UINT32 count, i;

	/* start by pushing the opcode */
	model2_3d_push(raster, opcode >> 23);

	/* push the starting address/dsp id */
	model2_3d_push(raster, *input++);

	/* get the count */
	count = *input++;

	/* push the count */
	model2_3d_push(raster, count);

	/* loop and send the data */
	for (i = 0; i < count; i++)
		model2_3d_push(raster, *input++);

	return input;
}

/* Command 05: Polygon Data */
static UINT32 *geo_polygon_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  address, count, i;
	UINT32 *p;

	(void)opcode;

	/* read in the address */
	address = *input++;

	/* prepare the pointer */
	if (address & 0x01000000)
	{
		/* Fast polygon RAM */
		p = &geo->polygon_ram1[address & 0x7fff];
	}
	else
	{
		/* Slow Polygon RAM */
		p = &geo->polygon_ram0[address & 0x7fff];
	}

	/* read the count */
	count = *input++;

	/* move the data */
	for (i = 0; i < count; i++)
		*p++ = *input++;

	return input;
}

/* Command 06: Texture Parameters */
static UINT32 *geo_texture_parameters(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  index, count, i, param;

	(void)opcode;

	/* read in the index */
	index = (*input++) >> 2;

	/* read in the conut */
	count = *input++;

	for (i = 0; i < count; i++)
	{
		/* read in the texture parameters */
		param = *input++;

		geo->texture_parameters[index].diffuse = (float)(param & 0xff);
		geo->texture_parameters[index].ambient = (float)((param >> 8) & 0xff);
		geo->texture_parameters[index].specular_control = (param >> 24) & 0xff;
		geo->texture_parameters[index].specular_scale = (float)((param >> 16) & 0xff);

		/* read in the distance coefficient */
		geo->coef_table[index] = u2f(*input++);

		index = (index + 1) & 0x1f;
	}

	return input;
}

/* Command 07: Geo Mode */
static UINT32 *geo_mode(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	(void)opcode;

	/* read in the mode */
	geo->mode = *input++;

	return input;
}

/* Command 08: ZSort Mode */
static UINT32 *geo_zsort_mode(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;

	/* push the opcode */
	model2_3d_push(raster, opcode >> 23);

	/* push the mode */
	model2_3d_push(raster, (*input++) >> 8);

	return input;
}

/* Command 09: Focal Distance */
static UINT32 *geo_focal_distance(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	(void)opcode;

	/* read the x focus value */
	geo->focus.x = u2f(*input++);

	/* read the y focus value */
	geo->focus.y = u2f(*input++);

	return input;
}

/* Command 0A: Light Source Vector Write */
static UINT32 *geo_light_source(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	(void)opcode;

	/* read the x light value */
	geo->light.x = u2f(*input++);

	/* read the y light value */
	geo->light.y = u2f(*input++);

	/* read the z light value */
	geo->light.pz = u2f(*input++);

	return input;
}

/* Command 0B: Transformation Matrix Write */
static UINT32 *geo_matrix_write(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  i;

	(void)opcode;

	/* read in the transformation matrix */
	for (i = 0; i < 12; i++)
		geo->matrix[i] = u2f(*input++);

	return input;
}

/* Command 0C: Parallel Transfer Vector Write */
static UINT32 *geo_translate_write(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  i;

	(void)opcode;

	/* read in the translation vector */
	for (i = 0; i < 3; i++)
		geo->matrix[i+9] = u2f(*input++);

	return input;
}

/* Command 0D: Geo Data Memory Push (undocumented, unsupported) */
static UINT32 *geo_data_mem_push(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  address, count, i;

	/*
	    This command pushes data stored in the Geometry DSP's RAM
	    to the hardware 3D rasterizer. Since we don't emulate the
	    DSP, we don't know what the RAM contents are.

	    Eventually, we could check for the address, and if it
	    happens to point to a polygon ROM, we could potentially
	    emulate it partially.

	    No games are known to use this command yet.
	*/


	(void)opcode;

	/* read in the address */
	address = *input++;

	/* read in the count */
	count = *input++;

	logerror("SEGA GEO: Executing unsupported geo_data_mem_push (address = %08x, count = %08x)\n", address, count);

	(void)i;
/*
    for (i = 0; i < count; i++)
        model2_3d_push(0);
*/

	return input;
}

/* Command 0E: Geo Test */
static UINT32 *geo_test(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32      data, blocks, address, count, checksum, i;

	(void)opcode;

	/* fifo test */
	data = 1;

	for (i = 0; i < 32; i++)
	{
		if (*input++ != data)
		{
			/* TODO: Set Red LED on */
			logerror("SEGA GEO: FIFO Test failed\n");
		}

		data <<= 1;
	}

	/* get the number of checksums we have to run */
	blocks = *input++;

	for (i = 0; i < blocks; i++)
	{
		UINT32  sum_even, sum_odd, j;

		/* read in the address */
		address = (*input++) & 0x7fffff;

		/* read in the count */
		count = *input++;

		/* read in the checksum */
		checksum = *input++;

		/* reset the checksum counters */
		sum_even = 0;
		sum_odd = 0;

		for (j = 0; j < count; j++)
		{
			data = geo->polygon_rom[address++];

			address &= geo->polygon_rom_mask;

			sum_even += data >> 16;
			sum_even &= 0xffff;

			sum_odd += data & 0xffff;
			sum_odd &= 0xffff;
		}

		sum_even += checksum >> 16;
		sum_even &= 0xffff;

		sum_odd += checksum & 0xffff;
		sum_odd &= 0xffff;

		if (sum_even != 0 || sum_odd != 0)
		{
			/* TODO: Set Green LED on */
			logerror("SEGA GEO: Polygon ROM Test failed\n");
		}
	}

	return input;
}

/* Command 0F: End */
static UINT32 *geo_end(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;

	(void)opcode;

	/* signal the end of this data block the rasterizer */
	model2_3d_push(raster, 0xff000000);

	/* signal end by returning nullptr */
	return NULL;
}

/* Command 10: Dummy */
static UINT32 *geo_dummy(geo_state *geo, UINT32 opcode, UINT32 *input)
{
/*  UINT32  data; */
	(void)opcode;

	/* do the dummy read cycle */
/*  data = *input++; */
	input++;

	return input;
}

/* Command 14: Log Data Write */
static UINT32 *geo_log_data(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	raster_state *raster = geo->raster;
	UINT32  i, count;

	/* start by pushing the opcode */
	model2_3d_push(raster, opcode >> 23);

	/* push the starting address/dsp id */
	model2_3d_push(raster, *input++);

	/* get the count */
	count = *input++;

	/* push the count */
	model2_3d_push(raster, count << 2);

	/* loop and send the data */
	for (i = 0; i < count; i++)
	{
		UINT32  data = *input++;

		model2_3d_push(raster, data & 0xff);
		model2_3d_push(raster, (data >> 8) & 0xff);
		model2_3d_push(raster, (data >> 16) & 0xff);
		model2_3d_push(raster, (data >> 24) & 0xff);
	}

	return input;
}

/* Command 16: LOD */
static UINT32 *geo_lod(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	(void)opcode;

	/* read in the LOD */
	geo->lod = u2f(*input++);

	return input;
}

/* Command 1D: Code Upload  (undocumented, unsupported) */
static UINT32 *geo_code_upload(geo_state *geo, UINT32 opcode, UINT32 *input)
{
	UINT32  count, i;

	/*
	    This command uploads code to program memory and
	    optionally runs it. Probably used for debugging.

	    No games are known to use this command yet.
	*/

	logerror("SEGA GEO: Uploading debug code (unimplemented)\n");

	(void)opcode;

	/* read in the flags */
/*  flags = *input++; */
	input++;

	/* read in the count */
	count = *input++;

	for (i = 0; i < count; i++)
	{
		UINT64  code;

		/* read the top part of the opcode */
		code = *input++;

		code <<= 32;

		/* the bottom part comes in two pieces */
		code |= *input++;
		code |= (*input++) << 16;
	}

	/*
	    Bit 10 of flags indicate whether to run iummediately after upload
	*/

/*
    if (flags & 0x400)
        code_jump();
*/

	return input;
}

/* Command 1E: Code Jump (undocumented, unsupported) */
static UINT32 *geo_code_jump(geo_state *geo, UINT32 opcode, UINT32 *input)
{
/*  UINT32  address; */

	/*
	    This command jumps to a specified address in program
	    memory. Code can be uploaded with function 1D.
	    Probably used for debugging.

	    No games are known to use this command yet.
	*/

	logerror("SEGA GEO: Jumping to debug code (unimplemented)\n");

	(void)opcode;

/*  address = *input++ & 0x3ff; */
	input++;

/*
    code_jump(address)
*/
	return input;
}

/*
    Top Skater uploads its own geometrizer program (MAME: "a slightly
    different geometry code"), whose object data command 11 reads a
    compact polygon list: the attribute word and then only the points,
    three words for a triangle and six for a quad, without the normal and
    the unused second point slot of the standard no-normals format (the
    lighting normal is computed from the points, as there).
    Command 1B loads a second matrix and 1C copies the translation of the
    first into it; a vertex with the lowest bit of x set goes through the
    second (the skinned joints of the skaters). Found as well in MAME pull
    request #16444 (jkind73, not merged), with the compact format.

    Bits 6-7 of the command word select the vanishing point (center) of
    the window, as in the rasterizer command: the speed meter is drawn
    around the second one, in the bottom left corner.
*/
int model2_geo_compact_objects;

static UINT32 *geo_process_command(geo_state *geo, UINT32 opcode, UINT32 *input, int *end_code)
{
	if( model2_geo_compact_objects )
	{
		int i;
		switch( ( opcode >> 23 ) & 0x1f )
		{
		case 0x11:
			geo->compact = 1;
			input = geo_object_data( geo, opcode, input );
			geo->compact = 0;
			return input;
		case 0x1b:	/* the second matrix */
			for( i = 0; i < 12; i++ )
				geo->matrix2[ i ] = u2f( *input++ );
			return input;
		case 0x1c:	/* the second matrix takes the translation of the first (no data) */
			for( i = 0; i < 3; i++ )
				geo->matrix2[ 9 + i ] = geo->matrix[ 9 + i ];
			return input;
		}
	}

	switch ((opcode >> 23) & 0x1f)
	{
		case 0x00: input = geo_nop(geo, opcode, input);                   break;
		case 0x01: input = geo_object_data(geo, opcode, input);           break;
		case 0x02: input = geo_direct_data(geo, opcode, input);           break;
		case 0x03: input = geo_window_data(geo, opcode, input);           break;
		case 0x04: input = geo_texture_data(geo, opcode, input);          break;
		case 0x05: input = geo_polygon_data(geo, opcode, input);          break;
		case 0x06: input = geo_texture_parameters(geo, opcode, input);    break;
		case 0x07: input = geo_mode(geo, opcode, input);                  break;
		case 0x08: input = geo_zsort_mode(geo, opcode, input);            break;
		case 0x09: input = geo_focal_distance(geo, opcode, input);        break;
		case 0x0a: input = geo_light_source(geo, opcode, input);          break;
		case 0x0b: input = geo_matrix_write(geo, opcode, input);          break;
		case 0x0c: input = geo_translate_write(geo, opcode, input);       break;
		case 0x0d: input = geo_data_mem_push(geo, opcode, input);         break;
		case 0x0e: input = geo_test(geo, opcode, input);                  break;
		case 0x0f: input = geo_end(geo, opcode, input); *end_code = 1; break;
		case 0x10: input = geo_dummy(geo, opcode, input);                 break;
		case 0x11: input = geo_object_data(geo, opcode, input);           break;
		case 0x12: input = geo_direct_data(geo, opcode, input);           break;
		case 0x13: input = geo_window_data(geo, opcode, input);           break;
		case 0x14: input = geo_log_data(geo, opcode, input);              break;
		case 0x15: input = geo_polygon_data(geo, opcode, input);          break;
		case 0x16: input = geo_lod(geo, opcode, input);                   break;
		case 0x17: input = geo_mode(geo, opcode, input);                  break;
		case 0x18: input = geo_zsort_mode(geo, opcode, input);            break;
		case 0x19: input = geo_focal_distance(geo, opcode, input);        break;
		case 0x1a: input = geo_light_source(geo, opcode, input);          break;
		case 0x1b: input = geo_matrix_write(geo, opcode, input);          break;
		case 0x1c: input = geo_translate_write(geo, opcode, input);       break;
		case 0x1d: input = geo_code_upload(geo, opcode, input);           break;
		case 0x1e: input = geo_code_jump(geo, opcode, input);             break;
		case 0x1f: input = geo_end(geo, opcode, input); *end_code = 1; break;
	}

	return input;
}

void model2_geo_parse( void )
{
	UINT32  address = (m_geo_read_start_address & 0x1ffff)/4;
	UINT32 *input = &model2_bufferram[address];
	UINT32  opcode;
	UINT32  op_count = 0;
	int end_code = 0;

	/* reset raster frame variables */
	render_frame_start();

	while (end_code == 0 && (input - model2_bufferram) < 0x20000/4 && op_count++ < 0x8000)
	{
		/* read in the opcode */
		opcode = *input++;

		/* if it's a jump opcode, do the jump */
		if (opcode & 0x80000000)
		{
			/* get the address */
			address = (opcode & 0x1ffff) / 4;

			/* update our pointer */
			input = &model2_bufferram[address];

			/* go again */
			continue;
		}

		/* process it */
		input = geo_process_command(m_geo, opcode, input, &end_code);
	}
}

/***********************************************************************************************/



/*************************************
 *  Rasterizer (model2rd.ipp of current MAME)
 *************************************/

#define DEST_W 512
#define DEST_H 512

/* drawn pixels carry an opaque marker in the top byte (the alpha of rgb_t in
   MAME), so a black pixel (dark shadows) is not taken as transparent */
#define M2_PIXEL_OPAQUE 0xff000000

static UINT32 *m_destmap;
static UINT8 *m_fillmap;

typedef struct
{
	UINT32 lumabase;
	UINT32 colorbase;
	UINT8 checker;
	UINT32 *texsheet[ 2 ];
	UINT32 texwidth, texheight, texx, texy;
	UINT8 texwrapx, texwrapy, texmirrorx, texmirrory;
	UINT8 utex, utexminlod;
	UINT32 utexx, utexy;
	INT32 texlod;
	UINT8 luma;
} m2_poly_extra_data;

static INLINE UINT16 get_texel( UINT32 base_x, UINT32 base_y, int x, int y, const UINT32 *sheet )
{
	int x2 = base_x + x;
	int y2 = base_y + y;
	UINT32 offset, texel;
	if( x2 >= 1024 )
	{
		/* texture sheets are mapped as 2048x1024 but stored in RAM as 1024x2048 */
		x2 -= 1024;
		y2 ^= 1024;
	}
	offset = ( ( y2 / 2 ) * 512 ) + ( x2 / 2 );
	texel = sheet[ ( offset >> 1 ) & 0x3ffff ];
	if( offset & 1 )
		texel >>= 16;
	if( ( y & 1 ) == 0 )
		texel >>= 8;
	if( ( x & 1 ) == 0 )
		texel >>= 4;
	return texel & 0x0f;
}

#define LERP( x, y, a )	( ( (x) + ( ( ( (y) - (x) ) * (a) ) >> 8 ) ) & 0x00ff00ff )

static UINT32 fetch_bilinear_texel( const m2_poly_extra_data *object, int translucent, INT32 miplevel, INT32 u, INT32 v )
{
	UINT32 tex_wrap_x = object->texwrapx;
	UINT32 tex_wrap_y = object->texwrapy;
	UINT32 tex_mirr_x = object->texmirrorx;
	UINT32 tex_mirr_y = object->texmirrory;
	UINT32 tex_width, tex_height, tex_x, tex_y;
	UINT32 *sheet;
	UINT32 ufrac, vfrac, u0, u1, v0, v1;
	UINT32 tex00, tex01, tex10, tex11, tex0x, tex1x;

	if( miplevel == -1 )
	{
		/* microtexture */
		tex_width = 128;
		tex_height = 128;
		tex_x = object->utexx;
		tex_y = object->utexy;
		sheet = object->texsheet[ 1 ];
		u <<= 1 << object->utexminlod;
		v <<= 1 << object->utexminlod;
	}
	else
	{
		tex_width = object->texwidth >> miplevel;
		tex_height = object->texheight >> miplevel;
		tex_x = ( ( object->texx - 2048 ) >> miplevel ) & 2047;
		tex_y = ( ( object->texy - 1024 ) >> miplevel ) & 1023;
		sheet = object->texsheet[ miplevel & 1 ];
		u >>= miplevel;
		v >>= miplevel;
	}

	if( tex_mirr_x && ( u & ( tex_width << 8 ) ) )
		u = ~u;
	if( tex_mirr_y && ( v & ( tex_height << 8 ) ) )
		v = ~v;

	/* subtract 1/2 texel */
	u -= 0x80;
	v -= 0x80;

	ufrac = u & 0xff;
	vfrac = v & 0xff;

	u0 = ( u >> 8 ) & ( tex_width - 1 );
	u1 = ( u0 + 1 ) & ( tex_width - 1 );
	v0 = ( v >> 8 ) & ( tex_height - 1 );
	v1 = ( v0 + 1 ) & ( tex_height - 1 );

	/* clamp the texture coordinates if smooth wrapping is not enabled */
	if( !tex_wrap_x && u1 == 0 )
	{
		if( ufrac >= 0x80 )
			u0 = u1, u1++, ufrac = 0;
		else
			u1 = u0, u0--, ufrac = 0x100;
	}
	if( !tex_wrap_y && v1 == 0 )
	{
		if( vfrac >= 0x80 )
			v0 = 0, v1++, vfrac = 0;
		else
			v1 = v0, v0--, vfrac = 0x100;
	}

	tex00 = get_texel( tex_x, tex_y, u0, v0, sheet ) << 4;
	tex01 = get_texel( tex_x, tex_y, u1, v0, sheet ) << 4;
	tex10 = get_texel( tex_x, tex_y, u0, v1, sheet ) << 4;
	tex11 = get_texel( tex_x, tex_y, u1, v1, sheet ) << 4;

	if( translucent )
	{
		/* pack the alpha components into the upper 16 bits */
		if( tex00 != 0xf0 ) tex00 |= 0x00800000;
		if( tex01 != 0xf0 ) tex01 |= 0x00800000;
		if( tex10 != 0xf0 ) tex10 |= 0x00800000;
		if( tex11 != 0xf0 ) tex11 |= 0x00800000;

		/* a transparent texel takes the luma of the neighbouring texel */
		if( tex00 == 0x000000f0 ) tex00 = tex01 & 0xff;
		if( tex01 == 0x000000f0 ) tex01 = tex00 & 0xff;
		if( tex10 == 0x000000f0 ) tex10 = tex11 & 0xff;
		if( tex11 == 0x000000f0 ) tex11 = tex10 & 0xff;
	}

	tex0x = LERP( tex00, tex01, ufrac );
	tex1x = LERP( tex10, tex11, ufrac );

	if( translucent )
	{
		if( tex0x == 0x000000f0 ) tex0x = tex1x & 0xff;
		if( tex1x == 0x000000f0 ) tex1x = tex0x & 0xff;
	}

	return LERP( tex0x, tex1x, vfrac );
}

static INLINE INT32 fast_log2( float value )
{
	static const UINT8 s_log2_table[ 128 ] =
	{
		  0,   2,   5,   8,  11,  14,  16,  19,  22,  25,  27,  30,  33,  35,  38,  40,
		 43,  46,  48,  51,  53,  56,  58,  61,  63,  65,  68,  70,  73,  75,  77,  80,
		 82,  84,  87,  89,  91,  93,  96,  98, 100, 102, 104, 106, 109, 111, 113, 115,
		117, 119, 121, 123, 125, 127, 129, 132, 134, 136, 138, 140, 141, 143, 145, 147,
		149, 151, 153, 155, 157, 159, 161, 162, 164, 166, 168, 170, 172, 173, 175, 177,
		179, 181, 182, 184, 186, 188, 189, 191, 193, 194, 196, 198, 200, 201, 203, 205,
		206, 208, 209, 211, 213, 214, 216, 218, 219, 221, 222, 224, 225, 227, 229, 230,
		232, 233, 235, 236, 238, 239, 241, 242, 244, 245, 247, 248, 250, 251, 253, 254
	};
	UINT32 ival;
	INT32 exp;
	if( value < 0.0f )
		return 0;
	ival = f2u( value ) >> 16;
	exp = ( ival >> 7 ) - 127;
	return ( exp << 8 ) | s_log2_table[ ival & 127 ];
}

static INLINE int count_leading_zeros32( UINT32 v )
{
	int n = 0;
	if( v == 0 )
		return 32;
	while( !( v & 0x80000000 ) ) { v <<= 1; n++; }
	return n;
}

/* one span of a polygon: pixels [startx, stopx) of a scanline, with the
   parameters (1/z, u/z, v/z) at the centre of pixel startx and per pixel */
static void draw_scanline_solid( int scanline, int startx, int stopx, const m2_poly_extra_data *object, int translucent )
{
	UINT32 *p = &m_destmap[ scanline * DEST_W ];
	UINT8 *fill = &m_fillmap[ scanline * DEST_W ];
	const UINT16 *colortable_r = &model2_colorxlat[ 0x0000 / 2 ];
	const UINT16 *colortable_g = &model2_colorxlat[ 0x4000 / 2 ];
	const UINT16 *colortable_b = &model2_colorxlat[ 0x8000 / 2 ];
	UINT32 color = object->colorbase;
	UINT8 luma;
	UINT32 tr, tg, tb;
	int x, dx;

	/* if it's translucent, there's nothing to render */
	if( translucent )
		return;

	luma = object->luma >> 2;
	color = model2_palram[ ( color + 0x1000 ) & 0x1fff ] & 0xffff;
	colortable_r += ( ( color >> 0 ) & 0x1f ) << 8;
	colortable_g += ( ( color >> 5 ) & 0x1f ) << 8;
	colortable_b += ( ( color >> 10 ) & 0x1f ) << 8;
	tr = model2_gamma_table[ colortable_r[ luma ] & 0xff ];
	tg = model2_gamma_table[ colortable_g[ luma ] & 0xff ];
	tb = model2_gamma_table[ colortable_b[ luma ] & 0xff ];
	color = M2_PIXEL_OPAQUE | ( tr << 16 ) | ( tg << 8 ) | tb;

	x = startx;
	dx = object->checker ? 2 : 1;
	if( object->checker && !( ( x ^ scanline ) & 1 ) )
		x++;

	for( ; x < stopx; x += dx )
	{
		if( fill[ x ] == 0 )
		{
			p[ x ] = color;
			fill[ x ] = 0xff;
		}
	}
}

static void draw_scanline_tex( int scanline, int startx, int stopx, float ooz, float uoz, float voz,
	float dooz, float duoz, float dvoz, const m2_poly_extra_data *object, int translucent )
{
	UINT32 *p = &m_destmap[ scanline * DEST_W ];
	UINT8 *fill = &m_fillmap[ scanline * DEST_W ];
	const UINT16 *colortable_r = &model2_colorxlat[ 0x0000 / 2 ];
	const UINT16 *colortable_g = &model2_colorxlat[ 0x4000 / 2 ];
	const UINT16 *colortable_b = &model2_colorxlat[ 0x8000 / 2 ];
	UINT32 colorbase = object->colorbase;
	UINT32 lumabase = object->lumabase;
	INT32 max_level;
	int x, dx;
	UINT32 minwh = object->texwidth < object->texheight ? object->texwidth : object->texheight;

	/* maximum mipmap level from the texture size; down to 2x2 */
	max_level = 30 - count_leading_zeros32( minwh );

	colorbase = model2_palram[ ( colorbase + 0x1000 ) & 0x1fff ] & 0x7fff;
	colortable_r += ( ( colorbase >> 0 ) & 0x1f ) << 8;
	colortable_g += ( ( colorbase >> 5 ) & 0x1f ) << 8;
	colortable_b += ( ( colorbase >> 10 ) & 0x1f ) << 8;

	x = startx;
	dx = 1;
	if( object->checker )
	{
		if( !( ( x ^ scanline ) & 1 ) )
		{
			x++;
			ooz += dooz;
			uoz += duoz;
			voz += dvoz;
		}
		dx = 2;
		dooz *= 2.0f;
		duoz *= 2.0f;
		dvoz *= 2.0f;
	}

	for( ; x < stopx; x += dx, ooz += dooz, uoz += duoz, voz += dvoz )
	{
		float z;
		INT32 mml, level, u, v;
		UINT32 t, tr, tg, tb;
		UINT8 luma;

		if( fill[ x ] > 0 )
			continue;

		z = 1.0f / ooz;
		mml = -object->texlod + fast_log2( z );
		level = mml >> 7;
		if( level < 0 ) level = 0;
		if( level > max_level ) level = max_level;

		u = (INT32)( uoz * z * 256.0f );
		v = (INT32)( voz * z * 256.0f );

		t = fetch_bilinear_texel( object, translucent, level, u, v );

		if( mml > 0 && level < max_level )
		{
			UINT32 t2 = fetch_bilinear_texel( object, translucent, level + 1, u, v );
			INT32 frac = ( mml & 127 ) << 1;
			t = LERP( t, t2, frac );
		}
		else if( object->utex && mml < 0 )
		{
			/* microtexture; blend up to almost 50% */
			UINT32 t2 = fetch_bilinear_texel( object, translucent, -1, u, v );
			INT32 frac = -mml >> object->utexminlod;
			if( frac > 127 ) frac = 127;
			t = LERP( t, t2, frac );
		}

		if( translucent )
		{
			/* if alpha is less than 50%, discard */
			if( t < 0x00400000 )
				continue;
			t &= 0xff;
		}

		/* 8 bits of filtered texel, 128 entries in the translator map */
		luma = (UINT8)( (UINT32)model2_lumaram[ ( lumabase + ( t >> 1 ) ) & 0x7fff ] * object->luma / 256 );
		if( luma > 0x3f )
			luma = 0x3f;

		tr = model2_gamma_table[ colortable_r[ luma ] & 0xff ];
		tg = model2_gamma_table[ colortable_g[ luma ] & 0xff ];
		tb = model2_gamma_table[ colortable_b[ luma ] & 0xff ];

		p[ x ] = M2_PIXEL_OPAQUE | ( tr << 16 ) | ( tg << 8 ) | tb;
		fill[ x ] = 0xff;
	}
}

/* scanline rasterization of a convex polygon, pixel centres at +0.5 */
static void rasterize_polygon( const poly_vertex *v, int n, int minx, int maxx, int miny, int maxy,
	const m2_poly_extra_data *extra, int textured, int translucent )
{
	float fminy = v[ 0 ].y, fmaxy = v[ 0 ].y;
	float dpdx[ 3 ] = { 0, 0, 0 };
	int i, y, ystart, ystop, best = -1;
	float bestarea = 0;

	for( i = 1; i < n; i++ )
	{
		if( v[ i ].y < fminy ) fminy = v[ i ].y;
		if( v[ i ].y > fmaxy ) fmaxy = v[ i ].y;
	}

	/* x gradients of the parameters, from the widest triangle of the fan */
	if( textured )
	{
		for( i = 1; i + 1 < n; i++ )
		{
			float a = ( v[ i ].x - v[ 0 ].x ) * ( v[ i + 1 ].y - v[ 0 ].y ) - ( v[ i + 1 ].x - v[ 0 ].x ) * ( v[ i ].y - v[ 0 ].y );
			if( fabs( a ) > bestarea ) { bestarea = fabs( a ); best = i; }
		}
		if( best < 0 || bestarea < 1e-6f )
			return;
		{
			const poly_vertex *a = &v[ 0 ], *b = &v[ best ], *c = &v[ best + 1 ];
			float area = ( b->x - a->x ) * ( c->y - a->y ) - ( c->x - a->x ) * ( b->y - a->y );
			int k;
			for( k = 0; k < 3; k++ )
				dpdx[ k ] = ( ( b->p[ k ] - a->p[ k ] ) * ( c->y - a->y ) - ( c->p[ k ] - a->p[ k ] ) * ( b->y - a->y ) ) / area;
		}
	}

	ystart = (int)ceil( fminy - 0.5f );
	ystop = (int)ceil( fmaxy - 0.5f );
	if( ystart < miny ) ystart = miny;
	if( ystop > maxy + 1 ) ystop = maxy + 1;

	for( y = ystart; y < ystop; y++ )
	{
		float yc = y + 0.5f;
		float xl = 1e30f, xr = -1e30f;
		float pl[ 3 ] = { 0, 0, 0 };
		int startx, stopx;

		for( i = 0; i < n; i++ )
		{
			const poly_vertex *a = &v[ i ], *b = &v[ ( i + 1 ) % n ];
			float t, x;
			if( ( yc < a->y && yc < b->y ) || ( yc >= a->y && yc >= b->y ) )
				continue;
			t = ( yc - a->y ) / ( b->y - a->y );
			x = a->x + ( b->x - a->x ) * t;
			if( x < xl )
			{
				int k;
				xl = x;
				for( k = 0; k < 3; k++ )
					pl[ k ] = a->p[ k ] + ( b->p[ k ] - a->p[ k ] ) * t;
			}
			if( x > xr )
				xr = x;
		}
		if( xl > xr )
			continue;

		startx = (int)ceil( xl - 0.5f );
		stopx = (int)ceil( xr - 0.5f );
		if( startx < minx ) startx = minx;
		if( stopx > maxx + 1 ) stopx = maxx + 1;
		if( startx >= stopx )
			continue;

		if( textured )
		{
			float off = ( startx + 0.5f ) - xl;
			draw_scanline_tex( y, startx, stopx,
				pl[ 0 ] + dpdx[ 0 ] * off, pl[ 1 ] + dpdx[ 1 ] * off, pl[ 2 ] + dpdx[ 2 ] * off,
				dpdx[ 0 ], dpdx[ 1 ], dpdx[ 2 ], extra, translucent );
		}
		else
			draw_scanline_solid( y, startx, stopx, extra, translucent );
	}
}

static void model2_3d_render( polygon *poly, const struct rectangle *cliprect )
{
	m2_poly_extra_data extra;
	UINT8 renderer = ( poly->texheader[ 0 ] >> 13 ) & 3;	/* bit 14 = textured, bit 13 = translucent */
	int minx, maxx, miny, maxy, i;

	/* calculate and clip to viewport */
	minx = poly->viewport[ 0 ] + m_xoffs;
	maxx = poly->viewport[ 2 ] + m_xoffs;
	miny = ( 384 - poly->viewport[ 3 ] ) + m_yoffs;
	maxy = ( 384 - poly->viewport[ 1 ] ) + m_yoffs;
	if( minx < cliprect->min_x ) minx = cliprect->min_x;
	if( maxx > cliprect->max_x ) maxx = cliprect->max_x;
	if( miny < cliprect->min_y ) miny = cliprect->min_y;
	if( maxy > cliprect->max_y ) maxy = cliprect->max_y;
	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx > DEST_W - 1 ) maxx = DEST_W - 1;
	if( maxy > DEST_H - 1 ) maxy = DEST_H - 1;
	if( minx > maxx || miny > maxy )
		return;

	memset( &extra, 0, sizeof( extra ) );
	extra.checker = ( poly->texheader[ 0 ] >> 15 ) & 1;
	extra.lumabase = ( poly->texheader[ 1 ] & 0xff ) << 7;
	extra.colorbase = ( poly->texheader[ 3 ] >> 6 ) & 0x3ff;
	extra.luma = poly->luma;
	extra.texlod = poly->texlod;

	if( renderer & 2 )
	{
		extra.texmirrorx = ( poly->texheader[ 0 ] >> 8 ) & 1;
		extra.texmirrory = ( poly->texheader[ 0 ] >> 9 ) & 1;
		/* disable smooth wrapping if mirroring is enabled */
		extra.texwrapx = ( poly->texheader[ 0 ] >> 6 ) & 1 & ~extra.texmirrorx;
		extra.texwrapy = ( poly->texheader[ 0 ] >> 7 ) & 1 & ~extra.texmirrory;
		extra.texsheet[ 0 ] = ( poly->texheader[ 2 ] & 0x1000 ) ? model2_textureram1 : model2_textureram0;
		extra.texsheet[ 1 ] = ( poly->texheader[ 2 ] & 0x1000 ) ? model2_textureram0 : model2_textureram1;
		extra.texwidth = 32 << ( ( poly->texheader[ 0 ] >> 0 ) & 0x7 );
		extra.texheight = 32 << ( ( poly->texheader[ 0 ] >> 3 ) & 0x7 );
		extra.texx = 32 * ( ( poly->texheader[ 2 ] >> 0 ) & 0x3f );
		extra.texy = 32 * ( ( poly->texheader[ 2 ] >> 6 ) & 0x1f );
		/* microtexture parameters */
		extra.utex = ( poly->texheader[ 0 ] >> 12 ) & 1;
		extra.utexminlod = ( poly->texheader[ 0 ] >> 10 ) & 3;
		extra.utexx = ( ( poly->texheader[ 2 ] >> 13 ) & 1 ) * 128;
		extra.utexy = ( ( poly->texheader[ 2 ] >> 14 ) & 3 ) * 128;

		for( i = 0; i < poly->num_vertices; i++ )
		{
			poly->v[ i ].pz = 1.0f / ( poly->v[ i ].pz + FLT_MIN );
			poly->v[ i ].pu = poly->v[ i ].pu * poly->v[ i ].pz * ( 1.0f / 8.0f );
			poly->v[ i ].pv = poly->v[ i ].pv * poly->v[ i ].pz * ( 1.0f / 8.0f );
		}
	}

	rasterize_polygon( poly->v, poly->num_vertices, minx, maxx, miny, maxy, &extra, ( renderer & 2 ) != 0, renderer & 1 );
}

/* 3D rasterizer projection: projects a polygon into screen coordinates */
static INLINE void model2_3d_project( polygon *poly )
{
	int i;
	for( i = 0; i < poly->num_vertices; i++ )
	{
		poly->v[ i ].x = m_crtc_xoffset + poly->center[ 0 ] + ( poly->v[ i ].x / ( poly->v[ i ].pz + FLT_MIN ) );
		poly->v[ i ].y = ( ( 384 - poly->center[ 1 ] ) + m_crtc_yoffset ) - ( poly->v[ i ].y / ( poly->v[ i ].pz + FLT_MIN ) );
	}
}

static void composite_3d( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	int x, y;
	for( y = cliprect->min_y; y <= cliprect->max_y && y < DEST_H; y++ )
	{
		UINT16 *dst = (UINT16 *)bitmap->line[ y ];
		const UINT32 *src = &m_destmap[ y * DEST_W ];
		for( x = cliprect->min_x; x <= cliprect->max_x && x < DEST_W; x++ )
		{
			UINT32 c = src[ x ];
			if( c )
				dst[ x ] = ( ( c >> 9 ) & 0x7c00 ) | ( ( c >> 6 ) & 0x03e0 ) | ( ( c >> 3 ) & 0x001f );
		}
	}
}

static void render_polygons( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	raster_state *raster = m_raster;
	int window, z, y;

	/* if the geometrizer hasn't presented a new frame, show the previous frame */
	if( m_render_done )
	{
		composite_3d( bitmap, cliprect );
		return;
	}

	/* if we have nothing to render, bail */
	if( raster->poly_list_index == 0 )
		return;

	for( y = cliprect->min_y; y <= cliprect->max_y && y < DEST_H; y++ )
	{
		memset( &m_destmap[ y * DEST_W ], 0, DEST_W * sizeof( UINT32 ) );
		memset( &m_fillmap[ y * DEST_W ], 0, DEST_W );
	}

	for( window = raster->cur_window; window >= 0; window-- )
	{
		/* go through the Z levels, and render each bucket */
		for( z = raster->min_z; z <= raster->max_z; z++ )
		{
			polygon *poly = raster->poly_sorted_list[ z ];
			while( poly != NULL )
			{
				if( poly->window == window )
				{
					model2_3d_project( poly );
					model2_3d_render( poly, cliprect );
				}
				poly = poly->next;
			}
		}
	}

	composite_3d( bitmap, cliprect );
	m_render_done = 1;
}


/*************************************
 *  Registers, video start / update
 *************************************/

WRITE32_HANDLER( model2_3d_zclip_w )
{
	/* setting this register to 0xff disables z-clip */
	m_raster->master_z_clip = data;
}

READ32_HANDLER( model2_polygon_count_r )
{
	return m_raster->poly_list_index;
}

/* the tile layers, mixed (see VIDEO_UPDATE) */
static struct mame_bitmap *m_tilemap;	/* the tile layers, mixed */
static UINT8 m_tile_front[ 256 ];

/* light gun crosshairs (Virtua Cop, Virtua Cop 2, The House of the Dead):
   the gun ports span the whole screen, as PORT_CROSSHAIR( X/Y, 1.0, 0.0 )
   in MAME. Found at video start from the input ports of the game */
static int m_gun_port[ 2 ][ 2 ];	/* [player][x, y]: port number, -1 = none */
static int m_gun_min[ 2 ][ 2 ], m_gun_max[ 2 ][ 2 ];

static void find_gun_ports( void )
{
	const struct InputPort *in = Machine->input_ports;
	int port = -1;
	int p, a;

	for( p = 0; p < 2; p++ )
		for( a = 0; a < 2; a++ )
			m_gun_port[ p ][ a ] = -1;

	for( ; in->type != IPT_END; in++ )
	{
		UINT32 type = in->type & ~IPF_MASK;
		if( type == IPT_PORT )
			port++;
		else if( ( type == IPT_LIGHTGUN_X || type == IPT_LIGHTGUN_Y ) &&
			in[ 1 ].type != IPT_END && ( in[ 1 ].type & ~IPF_MASK ) == IPT_EXTENSION )
		{
			p = ( in->type & IPF_PLAYERMASK ) / IPF_PLAYER2;
			a = ( type == IPT_LIGHTGUN_Y );
			if( p < 2 && in[ 1 ].default_value > in[ 1 ].mask )
			{
				m_gun_port[ p ][ a ] = port;
				m_gun_min[ p ][ a ] = in[ 1 ].mask;			/* PORT_ANALOG min */
				m_gun_max[ p ][ a ] = in[ 1 ].default_value;	/* PORT_ANALOG max */
			}
		}
	}
}

static void draw_gun_crosshairs( struct mame_bitmap *bitmap, const struct rectangle *cliprect )
{
	const struct rectangle *visible = &Machine->visible_area;
	int p;

	for( p = 0; p < 2; p++ )
	{
		int x, y;
		if( m_gun_port[ p ][ 0 ] < 0 || m_gun_port[ p ][ 1 ] < 0 )
			continue;
		x = ( readinputport( m_gun_port[ p ][ 0 ] ) & 0xff ) - m_gun_min[ p ][ 0 ];
		y = ( readinputport( m_gun_port[ p ][ 1 ] ) & 0xff ) - m_gun_min[ p ][ 1 ];
		x = visible->min_x + x * ( visible->max_x - visible->min_x ) / ( m_gun_max[ p ][ 0 ] - m_gun_min[ p ][ 0 ] );
		y = visible->min_y + y * ( visible->max_y - visible->min_y ) / ( m_gun_max[ p ][ 1 ] - m_gun_min[ p ][ 1 ] );
		draw_crosshair( p + 1, bitmap, x, y, cliprect );
	}
}

VIDEO_START( model2 )
{
	int i;

	if( sys24_tile_vh_start( 0x3fff ) )
		return 1;

	m_tilemap = auto_bitmap_alloc( Machine->drv->screen_width, Machine->drv->screen_height );
	if( !m_tilemap )
		return 1;
	/* priority bitmap value -> the winning tile pixel is in front of the polygons */
	for( i = 0; i < 256; i++ )
	{
		int b = 7;
		while( b > 0 && !( i & ( 1 << b ) ) )
			b--;
		m_tile_front[ i ] = b & 1;
	}

	m_raster = auto_malloc( sizeof( raster_state ) );
	m_geo = auto_malloc( sizeof( geo_state ) );
	memset( m_raster, 0, sizeof( raster_state ) );
	memset( m_geo, 0, sizeof( geo_state ) );
	m_raster->poly_list = auto_malloc( MAX_POLYGONS * sizeof( polygon ) );
	m_raster->poly_sorted_list = auto_malloc( 0x10000 * sizeof( polygon * ) );
	m_destmap = auto_malloc( DEST_W * DEST_H * sizeof( UINT32 ) );
	m_fillmap = auto_malloc( DEST_W * DEST_H );
	if( !m_raster || !m_geo || !m_raster->poly_list || !m_raster->poly_sorted_list || !m_destmap || !m_fillmap )
		return 1;
	memset( m_destmap, 0, DEST_W * DEST_H * sizeof( UINT32 ) );
	memset( m_fillmap, 0, DEST_W * DEST_H );

	m_raster->texture_rom = (UINT16 *)memory_region( REGION_USER4 );
	m_raster->texture_rom_mask = ( memory_region_length( REGION_USER4 ) / 2 ) - 1;
	m_geo->raster = m_raster;
	m_geo->polygon_rom = (UINT32 *)memory_region( REGION_USER3 );
	m_geo->polygon_rom_mask = ( memory_region_length( REGION_USER3 ) / 4 ) - 1;

	m_xoffs = 90;
	m_yoffs = -8;
	render_frame_start();
	find_gun_ports();

	state_save_register_UINT32( "model2video", 0, "geoctl", &m_geoctl, 1 );
	state_save_register_UINT32( "model2video", 0, "geocnt", &m_geocnt, 1 );
	state_save_register_UINT32( "model2video", 0, "geo_wr", &m_geo_write_start_address, 1 );
	state_save_register_UINT32( "model2video", 0, "geo_rd", &m_geo_read_start_address, 1 );
	state_save_register_int( "model2video", 0, "xoffset", &m_crtc_xoffset );
	state_save_register_int( "model2video", 0, "yoffset", &m_crtc_yoffset );
	state_save_register_INT16( "model2video", 0, "xoffs", &m_xoffs, 1 );
	state_save_register_INT16( "model2video", 0, "yoffs", &m_yoffs, 1 );
	state_save_register_UINT32( "model2video", 0, "command_buffer", m_raster->command_buffer, 32 );
	state_save_register_UINT32( "model2video", 0, "command_index", &m_raster->command_index, 1 );
	state_save_register_UINT32( "model2video", 0, "cur_command", &m_raster->cur_command, 1 );
	state_save_register_UINT8( "model2video", 0, "master_z_clip", &m_raster->master_z_clip, 1 );
	state_save_register_float( "model2video", 0, "polygon_z", &m_raster->polygon_z, 1 );
	state_save_register_INT32( "model2video", 0, "z_adjust", &m_raster->z_adjust, 1 );
	state_save_register_UINT32( "model2video", 0, "reverse", &m_raster->reverse, 1 );
	state_save_register_INT16( "model2video", 0, "viewport", m_raster->viewport, 4 );
	state_save_register_INT16( "model2video", 0, "center", &m_raster->center[ 0 ][ 0 ], 8 );
	state_save_register_UINT16( "model2video", 0, "center_sel", &m_raster->center_sel, 1 );
	state_save_register_UINT16( "model2video", 0, "texture_ram", m_raster->texture_ram, 0x10000 );
	state_save_register_UINT8( "model2video", 0, "log_ram", m_raster->log_ram, 0x8000 );
	state_save_register_float( "model2video", 0, "clip_plane", &m_raster->clip_plane[ 0 ][ 0 ].normal.x, sizeof( m_raster->clip_plane ) / sizeof( float ) );
	state_save_register_UINT32( "model2video", 0, "geo_mode", &m_geo->mode, 1 );
	state_save_register_float( "model2video", 0, "matrix", m_geo->matrix, 12 );
	state_save_register_float( "model2video", 0, "matrix2", m_geo->matrix2, 12 );
	state_save_register_float( "model2video", 0, "focus", &m_geo->focus.x, sizeof( poly_vertex ) / sizeof( float ) );
	state_save_register_float( "model2video", 0, "light", &m_geo->light.x, sizeof( poly_vertex ) / sizeof( float ) );
	state_save_register_float( "model2video", 0, "lod", &m_geo->lod, 1 );
	state_save_register_float( "model2video", 0, "coef_table", m_geo->coef_table, 32 );
	state_save_register_float( "model2video", 0, "texture_parameters", &m_geo->texture_parameters[ 0 ].diffuse, sizeof( m_geo->texture_parameters ) / ( sizeof( float ) ) );	/* all 32-bit fields */
	state_save_register_UINT32( "model2video", 0, "polygon_ram0", m_geo->polygon_ram0, 0x8000 );
	state_save_register_UINT32( "model2video", 0, "polygon_ram1", m_geo->polygon_ram1, 0x8000 );
	return 0;
}

/*
	Tile layers and polygons. The tile layers are mixed among themselves
	first, tilemap A over tilemap B, whatever their priority bits; the
	priority bit of the pixel that wins then puts it behind or in front of
	the polygons. Drawing each priority on its own (low tiles of A and B,
	polygons, high tiles of A and B) put a high tile of B over a low tile of
	A: on the Virtua Cop 2 stage select the city map (B, priority set) hid
	"SHOOT TO SELECT" (A).

	The tiles are drawn back to front into m_tilemap, each layer and
	priority setting its own bit in the priority bitmap: bit 2*depth+prio,
	with depth 0 for the back layer. The highest bit set gives the winning
	pixel and its priority.
*/
static void copy_tiles( struct mame_bitmap *bitmap, const struct rectangle *cliprect, int front )
{
	int x, y;
	for( y = cliprect->min_y; y <= cliprect->max_y; y++ )
	{
		UINT16 *dst = (UINT16 *)bitmap->line[ y ];
		const UINT16 *src = (const UINT16 *)m_tilemap->line[ y ];
		const UINT8 *pri = (const UINT8 *)priority_bitmap->line[ y ];
		for( x = cliprect->min_x; x <= cliprect->max_x; x++ )
			if( pri[ x ] && m_tile_front[ pri[ x ] ] == front )
				dst[ x ] = src[ x ];
	}
}

VIDEO_UPDATE( model2 )
{
	int layer;

	model2_palette_refresh();
	sys24_tile_update();

	fillbitmap( bitmap, Machine->pens[ 0 ], cliprect );
	fillbitmap( priority_bitmap, 0, cliprect );

	/* tilemap B (layers 3, 2), then A (1, 0) */
	for( layer = 3; layer >= 0; layer-- )
	{
		int depth = 3 - layer;
		sys24_tile_draw( m_tilemap, cliprect, layer << 1, depth * 2, 0 );
		sys24_tile_draw( m_tilemap, cliprect, ( layer << 1 ) | 1, depth * 2 + 1, 0 );
	}

	copy_tiles( bitmap, cliprect, 0 );
	render_polygons( bitmap, cliprect );
	copy_tiles( bitmap, cliprect, 1 );

	draw_gun_crosshairs( bitmap, cliprect );
}
