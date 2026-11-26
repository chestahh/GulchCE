/* Record emitted vertices and the registers inherited by the next immediate draw. */
#include "harness.h"

typedef struct { real_vector3d n; real d; } real_plane3d;
struct render_frustum { real_plane3d world_planes[6]; boolean projection_valid; };
static struct { struct render_frustum frustum; struct { boolean mirrored; } camera; } global_window_parameters;
struct decal_vertex { real_point3d position; unsigned long texcoord; };
static struct { struct decal_vertex *vertices; short vertex_count; byte color[4]; } decal_batch[4];
static short decal_batch_count;
static int inside, emitted, colors[16], current_color;
static real last_x, last_u;

static void position(real x)
{
	last_x = x;
	if (inside)
	{
		CHECK(emitted < 16, "extra vertices");
		colors[emitted++] = current_color;
	}
}

#define IDirect3DDevice8_Begin(device, type) (inside = TRUE)
#define IDirect3DDevice8_End(device) (inside = FALSE)
#define IDirect3DDevice8_SetVertexData4ub(device, reg, r, g, b, a) (current_color = (r))
#define IDirect3DDevice8_SetVertexData2f(device, reg, u, v) (last_u = (u))
#define IDirect3DDevice8_SetVertexData4f(device, reg, x, y, z, w) position(x)
#include "under_test.inc"

int main(int argc, char **argv)
{
	struct decal_vertex vertices[3][4] = {0};
	real_plane3d *planes = global_window_parameters.frustum.world_planes;
	int expected[3] = {1, 1, 1};
	int i, j, n = 0;

	CHECK(argc == 2, "missing case");
	global_window_parameters.frustum.projection_valid = TRUE;
	planes[0].n.i = -1;
	planes[1].n.i = 1;
	planes[2].n.j = -1;
	planes[3].n.j = 1;
	for (i = 0; i < 4; ++i)
		planes[i].d = 1;
	for (i = 0; i < 3; ++i)
	{
		decal_batch[i].vertices = vertices[i];
		decal_batch[i].vertex_count = 4;
		decal_batch[i].color[0] = (byte)(10 + i);
		for (j = 0; j < 4; ++j)
		{
			vertices[i][j].position.x = 2;
			vertices[i][j].position.y = (j & 1) ? -0.5f : 0.5f;
			vertices[i][j].texcoord = (unsigned long)(4*i+j);
		}
	}
	if (!strcmp(argv[1], "mixed"))
	{
		for (j = 0; j < 4; ++j)
			vertices[1][j].position.x = 0;
		expected[0] = expected[2] = 0;
	}
	else if (!strcmp(argv[1], "offscreen"))
		expected[0] = expected[1] = expected[2] = 0;
	else if (!strcmp(argv[1], "edge"))
	{
		for (i = 0; i < 3; ++i)
			vertices[i][3].position.x = 1.001f;
	}
	else if (!strcmp(argv[1], "enclosing"))
	{
		for (i = 0; i < 3; ++i)
		{
			vertices[i][0].position.x = vertices[i][3].position.x = -2;
			for (j = 0; j < 4; ++j)
				vertices[i][j].position.y = (j < 2) ? 2 : -2;
		}
	}
	else if (!strcmp(argv[1], "mirror"))
		global_window_parameters.camera.mirrored = TRUE;
	else if (!strcmp(argv[1], "no-projection"))
		global_window_parameters.frustum.projection_valid = FALSE;
	else
		return 2;

	decal_batch_count = 3;
	rasterizer_decals_draw_batch();
	for (i = 0; i < 3; ++i)
		if (expected[i])
			for (j = 0; j < 4; ++j)
			{
				CHECK(colors[n] == 10+i, "decal order at vertex %d", n);
				++n;
			}
	CHECK(emitted == n && !inside && !decal_batch_count, "wrong count or unfinished batch");
	CHECK(current_color == 12 && last_x == vertices[2][3].position.x &&
		last_u == 11/32767.0f, "lost the final attributes");
	return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
