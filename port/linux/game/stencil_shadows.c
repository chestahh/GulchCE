/* Optional desktop stencil volumes for nearby characters and vehicles.
The existing mesh reader supplies each posed surface; this module owns its
snapshots, welded edges and volume caches. Draw after transparent geometry,
before fog and the HUD, while the world depth buffer is available. */

#include "stencil_shadows.h"

#if defined(HALO_ARM64_GUEST) || defined(HALO_WEB) || defined(HALO_GLES)
void stencil_shadows_render(void) {}
void stencil_shadows_reset(void) {}
#else

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "objects/objects.h"
#include "render/render.h"
#include "rasterizer/rasterizer.h"
#include <xtl.h>
#include "rasterizer/xbox/rasterizer_xbox.h"

#include "game/players.h"
#include "units/units.h"
#include "physics/collisions.h"

#include "object_mesh.h"
#include "objects/object_definitions.h"
#include "rasterizer/xbox/rasterizer_xbox_vertex_shaders.h"
#include "rasterizer/xbox/rasterizer_xbox_pixel_shader.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* (the C library's own heap: cseries.h routes these to the game's debug
heap, whose free asserts on a NULL pointer) */
#undef malloc
#undef free
#undef realloc

/* port/linux/src/port_config.c */
int config_boolean(const char *name);
unsigned long config_changes(void);

/* render_objects.c (port): the objects drawn this frame */
short render_objects_rendered(long const **object_indices);
/* Save/restore the backend state without changing the game shader caches. */
void d3d8_gl_shadow_state(int restore);
/* port/linux/src/d3d8_gl.c: shadow volumes into the stencil, both faces in one draw */
void d3d8_gl_draw_shadow_volume(const float *positions, unsigned long vertex_count, int zfail);

/* ---------- constants */

enum
{
	STENCIL_SHADOW_MAXIMUM_OBJECTS = 16,
	STENCIL_SHADOW_MAXIMUM_MESHES = 16,
};

/* how far a volume runs from its caster, world units (about 75 m) */
#define STENCIL_SHADOW_EXTRUSION 25.f
/* the cap moved off the lit surface along the light, against flicker */
#define STENCIL_SHADOW_NUDGE 0.004f
/* casters this far from the camera and nearer (world units) */
#define STENCIL_SHADOW_RANGE 30.f
/* how far a volume runs past the first surface its light meets beyond the
caster (world units: about 2 m: its far end well under the surface,
not flickering against it), so it lands on that surface and no
further: not through a floor onto what is under it */
#define STENCIL_SHADOW_PAST_SURFACE 0.6f
/* what a shadowed pixel keeps of its colour */
#define STENCIL_SHADOW_KEEP 0.55f

/* ---------- structures */

struct stencil_shadow_edge
{
	unsigned short vertex[2]; /* (as the first triangle has it, in its order) */
	long triangle[2]; /* NONE: an open edge */
};

/* a mesh's edges, its vertices welded by position */
struct stencil_shadow_mesh
{
	struct object_mesh const *mesh; /* NULL: free */
	struct object_mesh surface;
	byte permutations[MAXIMUM_REGIONS_PER_OBJECT];
	short outward;
	long triangle_count;
	long edge_count;
	struct stencil_shadow_edge *edges;
	/* each vertex's welded one: every triangle drawn with these, so that
	triangles meeting at a seam meet exactly, as posed, and no crack opens */
	unsigned short *welded;
	unsigned long last_used;
};

/* ---------- globals */

static struct stencil_shadow_mesh stencil_shadow_meshes[STENCIL_SHADOW_MAXIMUM_MESHES];
static unsigned long stencil_shadow_frame = 0;

static real_point3d *stencil_shadow_world = NULL;
static long stencil_shadow_world_capacity = 0;
static boolean *stencil_shadow_lit = NULL;
static long stencil_shadow_lit_capacity = 0;

/* ---------- private code */

static boolean stencil_shadows_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = FALSE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("display.stencil_shadows") != 0;
	}
	return enabled;
}

struct stencil_shadow_weld
{
	real_point3d position;
	long vertex;
	long next;
};

/* its vertices welded (one id where they share a position: a seam's), and
each edge's two triangles */
static boolean stencil_shadow_build(
	struct stencil_shadow_mesh *entry,
	struct object_mesh const *mesh)
{
	long *weld = NULL;
	long *hash_heads = NULL;
	struct stencil_shadow_weld *welds = NULL;
	long *edge_heads = NULL;
	long *edge_next = NULL;
	long hash_size = 4096;
	long weld_count = 0;
	long index;
	boolean ok = FALSE;

	free(entry->edges);
	free(entry->welded);
	entry->edges = NULL;
	entry->welded = NULL;
	entry->edge_count = 0;
	entry->welded = malloc(sizeof(unsigned short) * mesh->vertex_count);
	weld = malloc(sizeof(long) * mesh->vertex_count);
	welds = malloc(sizeof(*welds) * mesh->vertex_count);
	hash_heads = malloc(sizeof(long) * hash_size);
	entry->edges = malloc(sizeof(struct stencil_shadow_edge) * mesh->triangle_count * 3);
	edge_heads = malloc(sizeof(long) * hash_size);
	edge_next = malloc(sizeof(long) * mesh->triangle_count * 3);
	if (!weld || !welds || !hash_heads || !entry->edges || !edge_heads || !edge_next || !entry->welded)
		goto done;
	for (index = 0; index < hash_size; index++)
		hash_heads[index] = edge_heads[index] = NONE;

	/* (welded where within a millimetre) */
	for (index = 0; index < mesh->vertex_count; index++)
	{
		real_point3d const *p = &mesh->vertices[index].position;
		long qx = (long)floor(p->x * 300.f);
		long qy = (long)floor(p->y * 300.f);
		long qz = (long)floor(p->z * 300.f);
		unsigned long hash = ((unsigned long)qx * 73856093u ^ (unsigned long)qy * 19349663u ^
			(unsigned long)qz * 83492791u) % (unsigned long)hash_size;
		long found = NONE;
		long at;

		for (at = hash_heads[hash]; at != NONE; at = welds[at].next)
		{
			if (fabs(welds[at].position.x - p->x) < 0.0004f && fabs(welds[at].position.y - p->y) < 0.0004f &&
				fabs(welds[at].position.z - p->z) < 0.0004f)
			{
				found = at;
				break;
			}
		}
		if (found == NONE)
		{
			found = weld_count++;
			welds[found].position = *p;
			welds[found].vertex = index;
			welds[found].next = hash_heads[hash];
			hash_heads[hash] = found;
		}
		weld[index] = found;
		entry->welded[index] = (unsigned short)welds[found].vertex;
	}

	/* each triangle's edges: a second triangle on the same welded pair (the
	other way round) shares it */
	for (index = 0; index < mesh->triangle_count; index++)
	{
		short side;

		for (side = 0; side < 3; side++)
		{
			unsigned short a = mesh->triangles[index][side];
			unsigned short b = mesh->triangles[index][(side + 1) % 3];
			long wa = weld[a];
			long wb = weld[b];
			unsigned long hash;
			long at;
			long found = NONE;

			if (wa == wb)
				continue;
			hash = ((unsigned long)MIN(wa, wb) * 2654435761u ^ (unsigned long)MAX(wa, wb)) % (unsigned long)hash_size;
			for (at = edge_heads[hash]; at != NONE; at = edge_next[at])
			{
				struct stencil_shadow_edge *edge = &entry->edges[at];

				if (edge->triangle[1] == NONE && weld[edge->vertex[0]] == wb && weld[edge->vertex[1]] == wa)
				{
					found = at;
					break;
				}
			}
			if (found != NONE)
			{
				entry->edges[found].triangle[1] = index;
			}
			else
			{
				struct stencil_shadow_edge *edge = &entry->edges[entry->edge_count];

				edge->vertex[0] = entry->welded[a];
				edge->vertex[1] = entry->welded[b];
				edge->triangle[0] = index;
				edge->triangle[1] = NONE;
				edge_next[entry->edge_count] = edge_heads[hash];
				edge_heads[hash] = entry->edge_count;
				entry->edge_count++;
			}
		}
	}
	entry->mesh = mesh;
	entry->triangle_count = mesh->triangle_count;
	ok = TRUE;

done:
	free(weld);
	free(welds);
	free(hash_heads);
	free(edge_heads);
	free(edge_next);
	if (!ok)
	{
		free(entry->edges);
		free(entry->welded);
		entry->edges = NULL;
		entry->welded = NULL;
		entry->mesh = NULL;
	}
	return ok;
}

/* The common reader keeps one transient mesh. Shadow snapshots belong here,
not in the first-person legs cache, and use the complete model/permutation key. */
static struct stencil_shadow_mesh *stencil_shadow_surface_get(
	long object_index)
{
	struct object_datum *object = object_try_and_get(object_index);
	struct object_definition *definition;
	struct object_mesh const *mesh;
	struct stencil_shadow_mesh *entry = &stencil_shadow_meshes[0];
	long model_index, index;
	double volume = 0.0;

	if (!object)
		return NULL;
	definition = object_definition_get(object->definition_index);
	model_index = definition ? definition->object.model.index : NONE;
	if (model_index == NONE)
		return NULL;
	for (index = 0; index < STENCIL_SHADOW_MAXIMUM_MESHES; index++)
	{
		struct stencil_shadow_mesh *candidate = &stencil_shadow_meshes[index];

		if (candidate->mesh && candidate->surface.model_index == model_index &&
			!memcmp(candidate->permutations, object->object.region_permutations, sizeof(candidate->permutations)))
		{
			candidate->last_used = stencil_shadow_frame;
			return candidate;
		}
		if (candidate->last_used < entry->last_used)
			entry = candidate;
	}
	mesh = object_mesh_get(object_index);
	if (!mesh || mesh->vertex_count <= 0 || mesh->vertex_count > OBJECT_MESH_MAXIMUM_VERTICES ||
		mesh->triangle_count <= 0 || mesh->triangle_count > OBJECT_MESH_MAXIMUM_TRIANGLES ||
		mesh->node_count <= 0 || mesh->node_count > OBJECT_MESH_MAXIMUM_NODES || !mesh->vertices || !mesh->triangles)
		return NULL;
	free(entry->surface.vertices);
	free(entry->surface.triangles);
	entry->mesh = NULL;
	entry->surface = *mesh;
	entry->surface.vertices = malloc(sizeof(*mesh->vertices) * mesh->vertex_count);
	entry->surface.triangles = malloc(sizeof(*mesh->triangles) * mesh->triangle_count);
	if (!entry->surface.vertices || !entry->surface.triangles)
		goto failed;
	memcpy(entry->surface.vertices, mesh->vertices, sizeof(*mesh->vertices) * mesh->vertex_count);
	memcpy(entry->surface.triangles, mesh->triangles, sizeof(*mesh->triangles) * mesh->triangle_count);
	for (index = 0; index < mesh->vertex_count; index++)
	{
		real_point3d const *p = &mesh->vertices[index].position;
		/* Bounds also make the welding hash's float-to-integer conversion safe. */
		if (!(p->x >= -1000000.f && p->x <= 1000000.f &&
			p->y >= -1000000.f && p->y <= 1000000.f &&
			p->z >= -1000000.f && p->z <= 1000000.f))
			goto failed;
	}
	for (index = 0; index < mesh->triangle_count; index++)
	{
		unsigned short const *t = mesh->triangles[index];
		real_point3d const *a, *b, *c;
		if (t[0] >= mesh->vertex_count || t[1] >= mesh->vertex_count || t[2] >= mesh->vertex_count)
			goto failed;
		a = &mesh->vertices[t[0]].position;
		b = &mesh->vertices[t[1]].position;
		c = &mesh->vertices[t[2]].position;
		volume += (double)a->x * ((double)b->y*c->z - (double)b->z*c->y) +
			(double)a->y * ((double)b->z*c->x - (double)b->x*c->z) +
			(double)a->z * ((double)b->x*c->y - (double)b->y*c->x);
	}
	entry->outward = volume < 0.0 ? -1 : 1;
	memcpy(entry->permutations, object->object.region_permutations, sizeof(entry->permutations));
	if (!stencil_shadow_build(entry, &entry->surface))
		goto failed;
	entry->last_used = stencil_shadow_frame;
	return entry;

failed:
	free(entry->surface.vertices);
	free(entry->surface.triangles);
	free(entry->edges);
	free(entry->welded);
	memset(entry, 0, sizeof(*entry));
	return NULL;
}

/* volumes: positions, three floats each, a triangle list */
struct stencil_shadow_buffer
{
	float *positions;
	unsigned long count; /* (vertices) */
	unsigned long capacity;
};

/* A whole draw must fit in the desktop renderer's 32 MB stream. */
#define STENCIL_SHADOW_BUFFER_MAXIMUM 900000ul

static boolean stencil_shadow_reserve(
	struct stencil_shadow_buffer *buffer,
	unsigned long count)
{
	if (buffer->count > STENCIL_SHADOW_BUFFER_MAXIMUM ||
		count > STENCIL_SHADOW_BUFFER_MAXIMUM - buffer->count)
		return FALSE;
	if (buffer->count + count > buffer->capacity)
	{
		unsigned long capacity = buffer->capacity ? buffer->capacity : 16384;
		float *grown;

		while (capacity < buffer->count + count)
			capacity = MIN(capacity * 2, STENCIL_SHADOW_BUFFER_MAXIMUM);
		grown = realloc(buffer->positions, capacity * 3 * sizeof(float));
		if (!grown)
			return FALSE;
		buffer->positions = grown;
		buffer->capacity = capacity;
	}
	return TRUE;
}

static void stencil_shadow_triangle(
	struct stencil_shadow_buffer *buffer,
	real_point3d const *a,
	real_point3d const *b,
	real_point3d const *c)
{
	float *out;

	if (!stencil_shadow_reserve(buffer, 3))
		return;
	out = &buffer->positions[buffer->count * 3];
	out[0] = a->x; out[1] = a->y; out[2] = a->z;
	out[3] = b->x; out[4] = b->y; out[5] = b->z;
	out[6] = c->x; out[7] = c->y; out[8] = c->z;
	buffer->count += 3;
}

static void stencil_shadow_append(
	struct stencil_shadow_buffer *buffer,
	struct stencil_shadow_buffer const *from)
{
	if (!from->count || !stencil_shadow_reserve(buffer, from->count))
		return;
	csmemcpy(&buffer->positions[buffer->count * 3], from->positions, from->count * 3 * sizeof(float));
	buffer->count += from->count;
}

/* a light: the sun's way (the game's shadow vector), and how far its
shadows run from what casts them */
struct stencil_shadow_light
{
	real_vector3d direction;
	real reach;
};

static void stencil_shadow_offset(
	struct stencil_shadow_light const *light,
	real_point3d const *p,
	real distance,
	real_point3d *result)
{
	result->x = p->x + light->direction.i * distance;
	result->y = p->y + light->direction.j * distance;
	result->z = p->z + light->direction.k * distance;
}

/* an object posed as drawn: its vertices in the world (only those the
volumes use: one of each welded set) */
static boolean stencil_shadow_pose(
	struct object_datum *object,
	struct object_mesh const *mesh,
	struct stencil_shadow_mesh const *edges,
	real_matrix4x3 const *nodes,
	long node_count)
{
	real_matrix4x3 skin[OBJECT_MESH_MAXIMUM_NODES];
	long index;

	(void)object;
	for (index = 0; index < mesh->node_count && index < OBJECT_MESH_MAXIMUM_NODES; index++)
		matrix4x3_multiply(&nodes[index < node_count ? index : 0], &mesh->inverse[index], &skin[index]);
	if (stencil_shadow_world_capacity < mesh->vertex_count)
	{
		real_point3d *grown = realloc(stencil_shadow_world, sizeof(real_point3d) * mesh->vertex_count);

		if (!grown)
			return FALSE;
		stencil_shadow_world = grown;
		stencil_shadow_world_capacity = mesh->vertex_count;
	}
	if (stencil_shadow_lit_capacity < mesh->triangle_count)
	{
		boolean *grown = realloc(stencil_shadow_lit, sizeof(boolean) * mesh->triangle_count);

		if (!grown)
			return FALSE;
		stencil_shadow_lit = grown;
		stencil_shadow_lit_capacity = mesh->triangle_count;
	}
	for (index = 0; index < mesh->vertex_count; index++)
	{
		struct object_mesh_vertex const *vertex = &mesh->vertices[index];
		short n0;
		short n1;
		real w;
		real_point3d p0;
		real_point3d p1;

		if (edges->welded[index] != index)
			continue;
		n0 = vertex->nodes[0] < mesh->node_count ? vertex->nodes[0] : 0;
		n1 = vertex->nodes[1] < mesh->node_count ? vertex->nodes[1] : 0;
		w = vertex->weight / 32767.f;
		matrix4x3_transform_point(&skin[n0], &vertex->position, &p0);
		if (w < 0.999f)
		{
			matrix4x3_transform_point(&skin[n1], &vertex->position, &p1);
			p0.x = p0.x * w + p1.x * (1.f - w);
			p0.y = p0.y * w + p1.y * (1.f - w);
			p0.z = p0.z * w + p1.z * (1.f - w);
		}
		stencil_shadow_world[index] = p0;
	}
	return TRUE;
}

/* one object's volume from the light: its sides (along the silhouette) and
its caps (the lit faces near, and far), apart: drawn "depth pass", where
the camera is outside the volume, the sides are enough */
static void stencil_shadow_object_volume(
	struct object_mesh const *mesh,
	struct stencil_shadow_mesh const *edges,
	struct stencil_shadow_light const *light,
	real nudge,
	struct stencil_shadow_buffer *sides,
	struct stencil_shadow_buffer *caps)
{
	long index;

	/* which face the light (out of the model: by the mesh's own winding) */
	for (index = 0; index < mesh->triangle_count; index++)
	{
		real_point3d const *a = &stencil_shadow_world[edges->welded[mesh->triangles[index][0]]];
		real_point3d const *b = &stencil_shadow_world[edges->welded[mesh->triangles[index][1]]];
		real_point3d const *c = &stencil_shadow_world[edges->welded[mesh->triangles[index][2]]];
		real_vector3d ab = { { b->x - a->x, b->y - a->y, b->z - a->z } };
		real_vector3d ac = { { c->x - a->x, c->y - a->y, c->z - a->z } };
		real_vector3d normal;

		cross_product3d(&ab, &ac, &normal);
		stencil_shadow_lit[index] = edges->outward * dot_product3d(&normal, &light->direction) < 0.f;
	}

	/* caps: the lit faces (nudged off the surface), and the same far away,
	turned to face out of the far end */
	for (index = 0; index < mesh->triangle_count; index++)
	{
		real_point3d near_points[3];
		real_point3d far_points[3];
		short corner;

		if (!stencil_shadow_lit[index])
			continue;
		for (corner = 0; corner < 3; corner++)
		{
			real_point3d const *p = &stencil_shadow_world[edges->welded[mesh->triangles[index][corner]]];

			stencil_shadow_offset(light, p, nudge, &near_points[corner]);
			/* (from the nudged point, as the sides run: a far cap and sides
			that do not meet leave cracks, drawn as lines across the ground) */
			stencil_shadow_offset(light, &near_points[corner], light->reach, &far_points[corner]);
		}
		if (edges->outward > 0)
		{
			stencil_shadow_triangle(caps, &near_points[0], &near_points[1], &near_points[2]);
			stencil_shadow_triangle(caps, &far_points[0], &far_points[2], &far_points[1]);
		}
		else
		{
			stencil_shadow_triangle(caps, &near_points[0], &near_points[2], &near_points[1]);
			stencil_shadow_triangle(caps, &far_points[0], &far_points[1], &far_points[2]);
		}
	}

	/* sides: along each edge of the lit faces not shared with another lit
	face, from the edge to far away */
	for (index = 0; index < edges->edge_count; index++)
	{
		struct stencil_shadow_edge const *edge = &edges->edges[index];
		boolean lit0 = stencil_shadow_lit[edge->triangle[0]];
		boolean lit1 = edge->triangle[1] != NONE && stencil_shadow_lit[edge->triangle[1]];
		real_point3d a, b, a_far, b_far;
		boolean forward;

		if (lit0 == lit1)
			continue;
		/* (the edge as the lit face runs it) */
		forward = lit0;
		stencil_shadow_offset(light, &stencil_shadow_world[edge->vertex[forward ? 0 : 1]], nudge, &a);
		stencil_shadow_offset(light, &stencil_shadow_world[edge->vertex[forward ? 1 : 0]], nudge, &b);
		stencil_shadow_offset(light, &a, light->reach, &a_far);
		stencil_shadow_offset(light, &b, light->reach, &b_far);
		if (edges->outward > 0)
		{
			stencil_shadow_triangle(sides, &a, &b_far, &b);
			stencil_shadow_triangle(sides, &a, &a_far, &b_far);
		}
		else
		{
			stencil_shadow_triangle(sides, &a, &b, &b_far);
			stencil_shadow_triangle(sides, &a, &b_far, &a_far);
		}
	}
}

/* how far a light's shadow of an object should run: to the first surface of
the level behind it along the light, and a little past */
static real stencil_shadow_reach(
	struct object_datum *object,
	long object_index,
	struct stencil_shadow_light const *light)
{
	struct collision_result collision;
	real_point3d const *center = &object->object.bounding_sphere_center;
	real_vector3d vector;
	real length = light->reach + object->object.bounding_sphere_radius;

	vector.i = light->direction.i * length;
	vector.j = light->direction.j * length;
	vector.k = light->direction.k * length;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		center, &vector, object_index, &collision))
	{
		real reach = collision.t * length + object->object.bounding_sphere_radius + STENCIL_SHADOW_PAST_SURFACE;

		return MIN(reach, light->reach);
	}
	return light->reach;
}

/* each caster's volume as last built, kept while nothing about it changes
(a parked vehicle, a body at rest, anyone standing still): built again only
when its pose, its light or its reach do */
struct stencil_shadow_cached
{
	long object_index; /* NONE: free */
	long model_index;
	byte permutations[MAXIMUM_REGIONS_PER_OBJECT];
	real_vector3d direction;
	real reach;
	long node_count;
	real_matrix4x3 nodes[OBJECT_MESH_MAXIMUM_NODES];
	struct stencil_shadow_buffer sides;
	struct stencil_shadow_buffer caps;
	unsigned long last_used;
};

static struct stencil_shadow_cached stencil_shadow_cache[STENCIL_SHADOW_MAXIMUM_OBJECTS + 4];
static struct stencil_shadow_buffer stencil_shadow_depth_fail; /* (sides and caps) */
static struct stencil_shadow_buffer stencil_shadow_depth_pass; /* (sides) */

static struct stencil_shadow_cached *stencil_shadow_cached_get(
	long object_index)
{
	struct stencil_shadow_cached *oldest = &stencil_shadow_cache[0];
	short index;

	for (index = 0; index < (short)NUMBEROF(stencil_shadow_cache); index++)
	{
		struct stencil_shadow_cached *entry = &stencil_shadow_cache[index];

		if (entry->object_index == object_index && entry->last_used != 0)
			return entry;
		if (entry->last_used < oldest->last_used)
			oldest = entry;
	}
	oldest->object_index = object_index;
	oldest->model_index = NONE;
	oldest->node_count = 0;
	return oldest;
}

/* whether the capsule from a to b, this thick, is outside the view */
static boolean stencil_shadow_outside_view(
	real_point3d const *a,
	real_point3d const *b,
	real radius)
{
	short plane;

	for (plane = 0; plane < 4; plane++)
	{
		real_plane3d const *p = &global_window_parameters.frustum.world_planes[plane];

		if (plane3d_distance_to_point(p, a) > radius && plane3d_distance_to_point(p, b) > radius)
			return TRUE;
	}
	return FALSE;
}

/* whether the camera may be inside the capsule from a to b, this thick */
static boolean stencil_shadow_camera_inside(
	real_point3d const *a,
	real_point3d const *b,
	real radius)
{
	real_point3d const *camera = &global_window_parameters.camera.position;
	real_vector3d ab = { { b->x - a->x, b->y - a->y, b->z - a->z } };
	real_vector3d ac = { { camera->x - a->x, camera->y - a->y, camera->z - a->z } };
	real length = dot_product3d(&ab, &ab);
	real t = length > 0.f ? dot_product3d(&ac, &ab) / length : 0.f;
	real_point3d nearest;

	t = PIN(t, 0.f, 1.f);
	nearest.x = a->x + ab.i * t;
	nearest.y = a->y + ab.j * t;
	nearest.z = a->z + ab.k * t;
	/* (and the near plane's breadth about it) */
	return distance3d(&nearest, camera) < radius + global_window_parameters.camera.z_near * 4.f + 0.05f;
}

/* an object's volume, from its cache or built anew, into the frame's */
static void stencil_shadow_object(
	long object_index,
	long own_unit)
{
	struct object_datum *object = object_try_and_get(object_index);
	struct stencil_shadow_mesh *edges = NULL;
	struct object_mesh const *mesh;
	struct render_lighting const *lighting;
	struct stencil_shadow_cached *cached;
	struct stencil_shadow_light sun;
	real_matrix4x3 const *nodes;
	long node_count;
	real radius;
	real_point3d far_center;
	boolean depth_fail;
	boolean own;

	if (!object)
		return;
	/* (what the local player holds: its shadow along their own, from their
	light; it is not drawn in first person, nor lit apart) */
	if (own_unit != NONE && object->object.parent_object_index == own_unit)
	{
		lighting = object_get_cached_render_lighting(own_unit, 100.f);
		own = TRUE;
	}
	else
	{
		lighting = object_get_cached_render_lighting(object_index, 100.f);
		own = object_index == own_unit;
	}
	if (!lighting)
		return;
	sun.direction = lighting->shadow_vector;
	if (normalize3d(&sun.direction) <= 0.f)
		return;
	sun.reach = STENCIL_SHADOW_EXTRUSION;
	sun.reach = stencil_shadow_reach(object, object_index, &sun);
	radius = object->object.bounding_sphere_radius;

	/* (its whole shadow out of the view: nothing to draw) */
	stencil_shadow_offset(&sun, &object->object.bounding_sphere_center, sun.reach, &far_center);
	if (!own &&
		stencil_shadow_outside_view(&object->object.bounding_sphere_center, &far_center, radius))
	{
		return;
	}
	/* (the camera in its shadow, or its own: "depth fail", caps and all;
	otherwise "depth pass", the sides alone) */
	depth_fail = own ||
		stencil_shadow_camera_inside(&object->object.bounding_sphere_center, &far_center, radius);

	edges = stencil_shadow_surface_get(object_index);
	if (!edges)
		return;
	mesh = edges->mesh;
	/* (as drawn this frame: between the last two ticks, as the renderer
	blends them; the tick's own pose stepped at 30 a second) */
	nodes = (real_matrix4x3 const *)object_get_node_matrices(object_index);
	node_count = object->object.node_matrices.size / (long)sizeof(real_matrix4x3);
	if (!nodes || node_count <= 0 || node_count > OBJECT_MESH_MAXIMUM_NODES)
		return;

	cached = stencil_shadow_cached_get(object_index);
	cached->last_used = stencil_shadow_frame;
	if (cached->model_index != mesh->model_index ||
		memcmp(cached->permutations, edges->permutations, sizeof(cached->permutations)) || cached->node_count != node_count ||
		csmemcmp(&cached->direction, &sun.direction, sizeof(sun.direction)) ||
		fabs(cached->reach - sun.reach) > 0.001f ||
		csmemcmp(cached->nodes, nodes, sizeof(real_matrix4x3) * node_count))
	{
		real nudge;

		cached->model_index = NONE;
		cached->sides.count = 0;
		cached->caps.count = 0;
		if (!stencil_shadow_pose(object, mesh, edges, nodes, node_count))
			return;
		/* (off the surface by its size: a vehicle's broad panels flickered at
		the nudge a character's needs) */
		nudge = MAX(STENCIL_SHADOW_NUDGE, radius * 0.012f);
		/* Reserve whole volumes before emitting triangles: partial caps leak stencil. */
		if (!stencil_shadow_reserve(&cached->sides, mesh->triangle_count * 18UL) ||
			!stencil_shadow_reserve(&cached->caps, mesh->triangle_count * 6UL))
			return;
		stencil_shadow_object_volume(mesh, edges, &sun, nudge, &cached->sides, &cached->caps);
		cached->model_index = mesh->model_index;
		memcpy(cached->permutations, edges->permutations, sizeof(cached->permutations));
		cached->node_count = node_count;
		cached->direction = sun.direction;
		cached->reach = sun.reach;
		csmemcpy(cached->nodes, nodes, sizeof(real_matrix4x3) * node_count);
	}
	if (depth_fail)
	{
		if (!stencil_shadow_reserve(&stencil_shadow_depth_fail, cached->sides.count + cached->caps.count))
			return;
		stencil_shadow_append(&stencil_shadow_depth_fail, &cached->sides);
		stencil_shadow_append(&stencil_shadow_depth_fail, &cached->caps);
	}
	else
	{
		stencil_shadow_append(&stencil_shadow_depth_pass, &cached->sides);
	}
}

/* the local player's unit (NONE: none) */
static long stencil_shadow_own_unit(
	void)
{
	long player_index = local_player_get_player_index(render.local_player_index);

	return player_index == NONE ? NONE : player_get(player_index)->unit_index;
}

/* the casters: characters and vehicles drawn this frame, near the camera,
and the local player's own unit, seen from inside it (its shadow cast, as
F.E.A.R. casts the player's) */
static short stencil_shadow_casters(
	long casters[STENCIL_SHADOW_MAXIMUM_OBJECTS],
	long own_unit)
{
	long const *drawn = NULL;
	short drawn_count = render_objects_rendered(&drawn);
	short count = 0;
	short index;

	if (own_unit != NONE && object_try_and_get(own_unit))
	{
		long child_index = object_get(own_unit)->object.first_child_object_index;

		casters[count++] = own_unit;
		/* (and what it holds: the weapon in its hands) */
		while (child_index != NONE && count < STENCIL_SHADOW_MAXIMUM_OBJECTS)
		{
			struct object_datum *child = object_try_and_get(child_index);

			if (!child)
				break;
			if (child->object.type == _object_type_weapon)
				casters[count++] = child_index;
			child_index = child->object.next_object_index;
		}
	}
	for (index = 0; index < drawn_count && count < STENCIL_SHADOW_MAXIMUM_OBJECTS; index++)
	{
		struct object_datum *object = object_try_and_get(drawn[index]);

		if (!object || drawn[index] == own_unit ||
			(own_unit != NONE && object->object.parent_object_index == own_unit) ||
			(object->object.type != _object_type_biped && object->object.type != _object_type_vehicle))
		{
			continue;
		}
		if (distance3d(&object->object.bounding_sphere_center, &global_window_parameters.camera.position) >
			STENCIL_SHADOW_RANGE + object->object.bounding_sphere_radius)
		{
			continue;
		}
		casters[count++] = drawn[index];
	}
	return count;
}

/* Select the existing untextured shaders directly: the game's permutation
cache must still describe the shader restored after this optional pass. */
static void stencil_shadow_begin(boolean screenspace)
{
	struct pixel_shader_definition shader;
	real constants[5][4] = {{0}};
	real width, height;

	memset(&shader, 0, sizeof(shader));
	shader.combiner_count = 1;
	shader.final_combiner_inputs_abcd = 4;
	rasterizer_set_pixel_shader(&shader);
	IDirect3DDevice8_SetVertexShader(global_d3d_device, vertex_shader_table[screenspace ? 4 : 0].handle);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHATESTENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZENABLE, screenspace ? D3DZB_FALSE : D3DZB_TRUE);
	if (screenspace)
	{
		width = global_window_parameters.camera.viewport_bounds.x1 - global_window_parameters.camera.viewport_bounds.x0;
		height = global_window_parameters.camera.viewport_bounds.y1 - global_window_parameters.camera.viewport_bounds.y0;
		constants[0][0] = 2.f / width;
		constants[0][3] = -1.f - 1.f / width;
		constants[1][1] = -2.f / height;
		constants[1][3] = 1.f + 1.f / height;
		constants[2][3] = 0.5f;
		constants[3][3] = 1.f;
		constants[4][0] = constants[4][1] = constants[4][3] = 1.f;
		IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, -68, constants, 5);
	}
}

/* ---------- public code */

void stencil_shadows_render(
	void)
{
	long casters[STENCIL_SHADOW_MAXIMUM_OBJECTS];
	short caster_count;
	short index;
	long own_unit;

	if (!stencil_shadows_enabled() || !global_d3d_device)
	{
		if (stencil_shadow_frame)
			stencil_shadows_reset();
		return;
	}
	if (++stencil_shadow_frame == 0)
	{
		stencil_shadows_reset();
		stencil_shadow_frame = 1;
	}
	own_unit = stencil_shadow_own_unit();
	caster_count = stencil_shadow_casters(casters, own_unit);
	if (caster_count == 0)
		return;

	stencil_shadow_depth_fail.count = 0;
	stencil_shadow_depth_pass.count = 0;
	for (index = 0; index < caster_count; index++)
		stencil_shadow_object(casters[index], own_unit);
	if (stencil_shadow_depth_fail.count == 0 && stencil_shadow_depth_pass.count == 0)
		return;

	d3d8_gl_shadow_state(FALSE);

	IDirect3DDevice8_Clear(global_d3d_device, 0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 0);

	/* the volumes into the stencil: a pixel inside any is left non-zero */
	stencil_shadow_begin(FALSE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZWRITEENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZFUNC, D3DCMP_LESS);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZBIAS, 0);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_COLORWRITEENABLE, 0);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILREF, 0);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILMASK, 0xff);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILWRITEMASK, 0xff);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	/* (both faces in one draw each: the volumes the camera is outside of
	counted where seen, the rest where hidden) */
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	d3d8_gl_draw_shadow_volume(stencil_shadow_depth_fail.positions, stencil_shadow_depth_fail.count, TRUE);
	d3d8_gl_draw_shadow_volume(stencil_shadow_depth_pass.positions, stencil_shadow_depth_pass.count, FALSE);

	/* the pixels inside darkened */
	stencil_shadow_begin(TRUE);
	{
		short width = global_window_parameters.camera.viewport_bounds.x1 - global_window_parameters.camera.viewport_bounds.x0;
		short height = global_window_parameters.camera.viewport_bounds.y1 - global_window_parameters.camera.viewport_bounds.y0;
		DWORD grey = (DWORD)(STENCIL_SHADOW_KEEP * 255.f);
		D3DCOLOR keep = (D3DCOLOR)(0xff000000u | grey << 16 | grey << 8 | grey);

		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ZWRITEENABLE, FALSE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_CULLMODE, D3DCULL_NONE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_SRCBLEND, D3DBLEND_ZERO);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILENABLE, TRUE);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILREF, 0);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILWRITEMASK, 0);
		IDirect3DDevice8_SetRenderState(global_d3d_device, D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		IDirect3DDevice8_Begin(global_d3d_device, D3DPT_TRIANGLELIST);
		IDirect3DDevice8_SetVertexDataColor(global_d3d_device, 9, keep);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, 0, 0);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, width, 0);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, width, height);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, 0, 0);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, width, height);
		IDirect3DDevice8_SetVertexData2s(global_d3d_device, 0, 0, height);
		IDirect3DDevice8_End(global_d3d_device);
	}

	IDirect3DDevice8_Clear(global_d3d_device, 0, NULL, D3DCLEAR_STENCIL, 0, 1.f, 0);

	d3d8_gl_shadow_state(TRUE);
}

void stencil_shadows_reset(
	void)
{
	short index;

	for (index = 0; index < STENCIL_SHADOW_MAXIMUM_MESHES; index++)
	{
		free(stencil_shadow_meshes[index].surface.vertices);
		free(stencil_shadow_meshes[index].surface.triangles);
		stencil_shadow_meshes[index].surface.vertices = NULL;
		stencil_shadow_meshes[index].surface.triangles = NULL;
		stencil_shadow_meshes[index].last_used = 0;
		free(stencil_shadow_meshes[index].edges);
		free(stencil_shadow_meshes[index].welded);
		stencil_shadow_meshes[index].edges = NULL;
		stencil_shadow_meshes[index].welded = NULL;
		stencil_shadow_meshes[index].mesh = NULL;
	}
	for (index = 0; index < (short)NUMBEROF(stencil_shadow_cache); index++)
	{
		free(stencil_shadow_cache[index].sides.positions);
		free(stencil_shadow_cache[index].caps.positions);
		csmemset(&stencil_shadow_cache[index], 0, sizeof(stencil_shadow_cache[index]));
		stencil_shadow_cache[index].object_index = NONE;
	}
	free(stencil_shadow_world);
	stencil_shadow_world = NULL;
	free(stencil_shadow_lit);
	stencil_shadow_lit = NULL;
	stencil_shadow_world_capacity = stencil_shadow_lit_capacity = 0;
	free(stencil_shadow_depth_fail.positions);
	free(stencil_shadow_depth_pass.positions);
	memset(&stencil_shadow_depth_fail, 0, sizeof(stencil_shadow_depth_fail));
	memset(&stencil_shadow_depth_pass, 0, sizeof(stencil_shadow_depth_pass));
	stencil_shadow_frame = 0;

}

#endif
