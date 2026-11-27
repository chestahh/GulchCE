/* Exercise the material walk with holes in the visible triangle buffer. */
#include "harness.h"

struct block { long count; void *address; };
struct ref { long index; };
struct structure_bsp { struct ref lightmap_group; struct block lightmaps; };
struct structure_lightmap { short bitmap_index; struct block materials; };
struct shader { struct { short type; } base; };
struct bitmap_data { int unused; };
struct vertex_buffer { int id; };
typedef struct { float data[4]; } real_plane3d;
struct structure_material {
	struct ref shader;
	long first_surface_index, surface_count;
	short breakable_surface_index, permutation_index, flags;
	struct vertex_buffer vertices;
	real_point3d centroid;
	real_plane3d plane;
	int lighting;
};
static struct structure_bsp bsp;
static struct shader test_shader;
static struct { real_vector3d fog_offset_vector; } structure_render_globals;
static int draws;
#define global_zero_vector3d NULL
#define global_structure_bsp_get() (&bsp)
#define TAG_BLOCK_GET_ELEMENT(block, index, type) (&((type *)(block)->address)[index])
#define bitmap_group_try_and_get_bitmap(group, bitmap) NULL
#define shader_definition_get(index) (&test_shader)
#define shader_type_is_transparent(type) FALSE
#define breakable_surface_extant(index) TRUE
enum { _structure_material_coplanar_bit, _structure_material_fog_plane_bit };
#include "under_test.inc"

static void draw(struct shader const *s, short permutation, long buffer, long first,
	long count, struct vertex_buffer const *vertices)
{
	static long const offsets[] = {1, 4};
	static long const counts[] = {2, 1};
	CHECK(draws < 2, "unexpected draw");
	CHECK(vertices->id == draws && first == offsets[draws] && count == counts[draws],
		"draw %d used buffer %d, offset %ld, count %ld", draws, vertices->id, first, count);
	draws++;
}

int main(void)
{
	struct structure_lightmap groups[3] = {0};
	struct structure_material first[2] = {0}, last = {0};
	long visible[] = {0, 2, 3, 5, 7, 10};

	first[0].first_surface_index = 2;
	first[0].surface_count = 2;
	/* first[1] was disabled by validation. Group 1 has no materials. */
	groups[0].materials = (struct block){2, first};
	last.first_surface_index = 7;
	last.surface_count = 1;
	last.vertices.id = 1;
	groups[2].materials = (struct block){1, &last};
	bsp.lightmaps = (struct block){3, groups};
	bsp.lightmap_group.index = NONE;
	structure_render_pass(visible, 6, 0, NULL, draw, NULL, NULL);
	CHECK(draws == 2, "expected both owned ranges");
	return 0;
}
