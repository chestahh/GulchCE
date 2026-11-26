/* Exercise the real surface selection with identifiable 2D query results. */
#include "harness.h"
#include <limits.h>
#include <math.h>
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&((t *)(b)->address)[i])
enum { COLLISION_VECTOR_CACHE_NODES=512 };
enum { _x, _y, _z };
struct block { long count; void *address; };
typedef struct { real_vector3d n; real d; } real_plane3d;
typedef struct { real x,y; } real_point2d;
struct collision_bsp { struct block leaves,bsp2d_references; struct { struct block planes; } bsp3d; struct { struct block nodes; } bsp2d; };
struct collision_leaf { short bsp2d_reference_count; long first_bsp2d_reference_index; };
struct bsp2d_reference { long plane_designator,root_index; };
#include "cache.inc"
static boolean projection_sign_from_vector3d(const real_vector3d *normal, short axis)
{
	return TRUE;
}

static void project_point3d(const real_point3d *point, short axis, boolean sign, real_point2d *out)
{
	out->x = point->x;
	out->y = point->y;
}

/* Each reference leads to a distinct surface. The first is breakable. */
static long bsp2d_test_point(const struct block *nodes, const real_point2d *point, long root)
{
	return root;
}

static boolean collision_bsp_valid_surface_index(const struct collision_bsp *bsp, long surface)
{
	return surface >= 0 && surface < 3;
}

static boolean collision_surface_test_point(const struct collision_bsp *bsp, short count, const byte *flags,
	long surface, short axis, boolean sign, const real_point2d *point)
{
	return flags[surface] && point->x >= 0;
}

#include "under_test.inc"

int main(void)
{
	struct collision_bsp_vector_cache cache = {0};
	struct collision_leaf leaf = {4, 0};
	struct bsp2d_reference refs[4] = {{2,2}, {1,0}, {1,1}, {2,2}};
	real_plane3d planes[3] = {0};
	struct collision_bsp bsp = {{1,&leaf}, {4,refs}, {{3,planes}}, {{0,NULL}}};
	real_point3d origin = {0,0,0};
	real_vector3d direction = {1,0,0};
	byte bits[3] = {0,1,1};
	int pass, plane;

	for (pass = 0; pass < 4; pass++)
	{
		long expected = pass == 2 ? NONE : (pass == 1 ? 0 : 1);
		bits[0] = pass == 1;
		direction.i = pass == 2 ? -1.f : 1.f;
		for (plane = 0; plane < 3; plane++)
		{
			long stock = collision_leaf_test_vector(&bsp, 3, bits, &origin, &direction, 0, plane, .5f, TRUE, NULL);
			long cached = collision_leaf_test_vector(&bsp, 3, bits, &origin, &direction, 0, plane, .5f, TRUE, &cache);
			CHECK(cached == stock, "cached scan preserves missing and duplicate references");
			if (plane == 1) CHECK(cached == expected, "ray and breakable tests are evaluated again");
		}
	}
	return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
