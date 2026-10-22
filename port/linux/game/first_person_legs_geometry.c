/*
FIRST_PERSON_LEGS_GEOMETRY.C

Private first-person triangle selection and surface clearance. Head/arm
triangles are omitted; original vertices, palettes, weights and animation
are untouched. Torso triangles stay intact instead of collapsing into a
waist pouch. All retained non-leg vertices, and vertices shared with omitted
triangles, constrain the pose's setback. Their animated positions stay
outside the view even at vertical pitch, so cuts never become visible.

The map loader validates the source geometry. Counts, formats, indices and
palettes are checked again here before reading. Private buffers survive
queued material/shield draws, have a fixed memory budget, and are released
on disable or map teardown. No buffer is evicted during a frame.
*/
#include "cseries.h"
#include "real_math.h"
#include "models/model_definitions.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer_model_types.h"
#include "first_person_legs_geometry.h"
#include <xtl.h>
#include <math.h>

enum { FIRST_PERSON_LEGS_PARTS = 256, FIRST_PERSON_LEGS_VERTICES = 65535,
	FIRST_PERSON_LEGS_TRIANGLES = 65535, FIRST_PERSON_LEGS_BYTES = 16 * 1024 * 1024 };

extern short custom_edition_part_palette(struct vertex_buffer const *, byte const **);
extern short mcc_cache_part_palette(void const *, byte const **);
extern void platform_log(char const *, ...);

struct first_person_legs_boundary
{
	real_point3d local[2];
	short nodes[2];
	real weight;
};

/* The engine's validated model-part layout (models.c). No map-family
conversion or buffer ownership changes here. */
struct first_person_legs_geometry
{
	byte reserved[0x24];
	struct tag_block parts;
};
struct first_person_legs_source_part
{
	unsigned long flags;
	byte reserved[0x1C];
	struct tag_block uncompressed_vertices, compressed_vertices, triangles;
	struct triangle_buffer triangle_buffer;
	struct vertex_buffer vertex_buffer;
};
typedef char first_person_legs_part_layout[sizeof(struct first_person_legs_source_part) == 0x68 ? 1 : -1];

static struct first_person_legs_part
{
	long definition_index;
	struct vertex_buffer const *source_vertices;
	struct triangle_buffer const *source_triangles;
	struct triangle_buffer triangles;
	word *indices;
	struct first_person_legs_boundary *boundary;
	long boundary_count;
	boolean valid;
} first_person_legs_parts[FIRST_PERSON_LEGS_PARTS];
static short first_person_legs_part_count;
static unsigned long first_person_legs_bytes;

static boolean first_person_legs_vertex_kept(
	struct model_vertex_compressed const *vertex, short node_count,
	boolean const *upper, short palette_count, byte const *palette)
{
	short weights[2];
	short index;

	if (vertex->node_weight < 0)
		return FALSE;
	weights[0] = vertex->node_weight;
	weights[1] = 32767 - weights[0];
	for (index = 0; index < 2; index++)
	{
		short node;

		if (weights[index] == 0)
			continue;
		if (vertex->nodes[index] % 3)
			return FALSE;
		node = vertex->nodes[index] / 3;
		if (palette_count)
		{
			if (node >= palette_count)
				return FALSE;
			node = palette[node];
		}
		if (node >= node_count || upper[node])
			return FALSE;
	}
	return TRUE;
}

static long first_person_legs_select(
	word const *source, long triangle_count, short type,
	boolean const *kept, long vertex_count, word *output)
{
	long triangle, count = 0;

	for (triangle = 0; triangle < triangle_count; triangle++)
	{
		long start = type == _triangle_buffer_type_triangles ? triangle * 3 : triangle;
		word a = source[start], b = source[start + 1], c = source[start + 2];

		if (a >= vertex_count || b >= vertex_count || c >= vertex_count)
			return NONE;
		if (a == b || a == c || b == c || !kept[a] || !kept[b] || !kept[c])
			continue;
		/* Odd strip triangles reverse winding. Degenerates still consume
		a position in the strip. */
		output[count * 3] = type == _triangle_buffer_type_precompiled_strip && (triangle & 1) ? b : a;
		output[count * 3 + 1] = type == _triangle_buffer_type_precompiled_strip && (triangle & 1) ? a : b;
		output[count * 3 + 2] = c;
		count++;
	}
	return count;
}

struct triangle_buffer const *first_person_legs_geometry_get(
	long definition_index, struct model const *model, boolean const *upper, boolean const *lower,
	struct vertex_buffer const *vertices, struct triangle_buffer const *triangles)
{
	struct first_person_legs_part *part;
	struct model_vertex_compressed const *data = NULL;
	word const *source = NULL;
	boolean *kept, *used, *edge;
	long node_count = model->nodes.count;
	byte const *palette = NULL, *mcc_palette = NULL;
	short palette_count, mcc_count, index;
	long vertex, count;
	unsigned long allocation;

	if (!triangles->count)
		return NULL;
	for (index = 0; index < first_person_legs_part_count; index++)
	{
		part = &first_person_legs_parts[index];
		if (part->definition_index == definition_index &&
			part->source_vertices == vertices && part->source_triangles == triangles)
			return part->triangles.count ? &part->triangles : NULL;
	}
	if (first_person_legs_part_count == FIRST_PERSON_LEGS_PARTS)
		return NULL;
	part = &first_person_legs_parts[first_person_legs_part_count++];
	csmemset(part, 0, sizeof(*part));
	part->definition_index = definition_index;
	part->source_vertices = vertices;
	part->source_triangles = triangles;
	if (node_count <= 0 || node_count > 64 ||
		vertices->type != _rasterizer_vertex_type_model_compressed ||
		vertices->count <= 0 || vertices->count > FIRST_PERSON_LEGS_VERTICES ||
		triangles->count <= 0 || triangles->count > FIRST_PERSON_LEGS_TRIANGLES ||
		(triangles->type != _triangle_buffer_type_triangles &&
		 triangles->type != _triangle_buffer_type_precompiled_strip) ||
		!vertices->hardware_format || !triangles->hardware_format)
	{
		platform_log("first-person legs: unsupported geometry; part not drawn\n");
		return NULL;
	}
	palette_count = custom_edition_part_palette(vertices, &palette);
	mcc_count = mcc_cache_part_palette(vertices, &mcc_palette);
	if (mcc_count)
	{
		palette_count = mcc_count;
		palette = mcc_palette;
	}
	if (palette_count < 0 || palette_count > 64 || (palette_count && !palette))
		return NULL;
	allocation = vertices->count * sizeof(*part->boundary) + triangles->count * 3 * sizeof(*part->indices);
	if (allocation > FIRST_PERSON_LEGS_BYTES - first_person_legs_bytes)
		return NULL;
	kept = malloc(vertices->count * 3 * sizeof(*kept));
	used = kept ? kept + vertices->count : NULL;
	edge = kept ? used + vertices->count : NULL;
	part->boundary = malloc(vertices->count * sizeof(*part->boundary));
	part->indices = malloc(triangles->count * 3 * sizeof(*part->indices));
	if (!kept || !part->indices || !part->boundary)
	{
		if (kept) free(kept);
		if (part->indices) free(part->indices);
		if (part->boundary) free(part->boundary);
		part->boundary = NULL;
		part->indices = NULL;
		return NULL;
	}
	first_person_legs_bytes += allocation;
	IDirect3DVertexBuffer8_Lock(vertices->hardware_format, 0,
		vertices->count * sizeof(*data), (BYTE **)&data, D3DLOCK_READONLY);
	IDirect3DIndexBuffer8_Lock(triangles->hardware_format, 0,
		(triangles->type == _triangle_buffer_type_triangles ? triangles->count * 3 : triangles->count + 2) * sizeof(word),
		(BYTE **)&source, D3DLOCK_READONLY);
	count = NONE;
	if (data && source)
	{
		for (vertex = 0; vertex < vertices->count; vertex++)
			kept[vertex] = first_person_legs_vertex_kept(&data[vertex], node_count, upper, palette_count, palette);
		count = first_person_legs_select(source, triangles->count, triangles->type, kept, vertices->count, part->indices);
		if (count >= 0)
		{
			long triangle;
			csmemset(used, 0, vertices->count * 2 * sizeof(*used));
			for (triangle = 0; triangle < triangles->count; triangle++)
			{
				long start = triangles->type == _triangle_buffer_type_triangles ? triangle * 3 : triangle;
				word a = source[start], b = source[start + 1], c = source[start + 2];
				boolean *mark;
				if (a == b || a == c || b == c) continue;
				mark = kept[a] && kept[b] && kept[c] ? used : edge;
				mark[a] = mark[b] = mark[c] = TRUE;
			}
			for (vertex = 0; vertex < vertices->count; vertex++)
			{
				struct first_person_legs_boundary *boundary = &part->boundary[part->boundary_count];
				short j;
				boolean leg_only = TRUE;
				if (!used[vertex]) continue;
				boundary->weight = data[vertex].node_weight / 32767.f;
				for (j = 0; j < 2; j++)
				{
					short node = data[vertex].nodes[j] / 3;
					real weight = j ? 1.f - boundary->weight : boundary->weight;
					if (weight == 0.f) node = 0;
					else if (palette_count) node = palette[node];
					boundary->nodes[j] = node;
					if (weight > 0.f && !lower[node]) leg_only = FALSE;
					matrix4x3_transform_point(&TAG_BLOCK_GET_ELEMENT(&model->nodes, node,
						struct model_node)->runtime_default_inverse_matrix, &data[vertex].position, &boundary->local[j]);
				}
				if (edge[vertex] || !leg_only) part->boundary_count++;
			}
			part->valid = TRUE;
		}

	}
	IDirect3DVertexBuffer8_Unlock(vertices->hardware_format);
	IDirect3DIndexBuffer8_Unlock(triangles->hardware_format);
	free(kept);
	if (count <= 0 || !rasterizer_triangle_buffer_new(&part->triangles,
		_triangle_buffer_type_triangles, count, part->indices))
	{
		free(part->indices);
		part->indices = NULL;
		free(part->boundary);
		part->boundary = NULL;
		part->boundary_count = 0;
		first_person_legs_bytes -= allocation;
		if (count > 0) part->valid = FALSE;
		return NULL;
	}
	return &part->triangles;
}

void first_person_legs_geometry_reset(void)
{
	short index;

	for (index = 0; index < first_person_legs_part_count; index++)
	{
		rasterizer_triangle_buffer_delete(&first_person_legs_parts[index].triangles);
		if (first_person_legs_parts[index].boundary)
			free(first_person_legs_parts[index].boundary);
		if (first_person_legs_parts[index].indices)
			free(first_person_legs_parts[index].indices);
	}
	csmemset(first_person_legs_parts, 0, sizeof(first_person_legs_parts));
	first_person_legs_part_count = 0;
	first_person_legs_bytes = 0;
}

boolean first_person_legs_geometry_prepare(long definition_index, struct model const *model,
	boolean const *upper, boolean const *lower)
{
	long geometry, part;
	short index;
	if (model->geometries.count < 0 || model->geometries.count > MAXIMUM_GEOMETRIES_PER_MODEL)
		return FALSE;
	for (geometry = 0; geometry < model->geometries.count; geometry++)
	{
		struct first_person_legs_geometry const *g = TAG_BLOCK_GET_ELEMENT(&model->geometries,
			geometry, struct first_person_legs_geometry);
		if (g->parts.count < 0 || g->parts.count > MAXIMUM_PARTS_PER_MODEL_GEOMETRY)
			return FALSE;
		for (part = 0; part < g->parts.count; part++)
		{
			struct first_person_legs_source_part const *p = TAG_BLOCK_GET_ELEMENT(&g->parts,
				part, struct first_person_legs_source_part);
			if (p->flags & 1) continue; /* stripped parts are not rendered */
			if (p->triangle_buffer.count == 0) continue;
			first_person_legs_geometry_get(definition_index, model, upper, lower, &p->vertex_buffer, &p->triangle_buffer);
		}
	}
	for (index = 0; index < first_person_legs_part_count; index++)
		if (first_person_legs_parts[index].definition_index == definition_index && !first_person_legs_parts[index].valid)
			return FALSE;
	return first_person_legs_part_count < FIRST_PERSON_LEGS_PARTS;
}

real first_person_legs_geometry_setback(long definition_index, real_matrix4x3 const *nodes,
	real_point3d const *pivot, real cosine, real sine, real_vector3d const *view,
	real_point3d const *eye, real vertical_fov, real minimum)
{
	real tangent, margin;
	short part;
	real result = minimum;
	if (!(vertical_fov > 0.f && vertical_fov < _pi)) return REAL_MAX;
	/* At vertical pitch a surface point is outside the view when its
	rearward distance exceeds abs(height) * tan(half FOV). That also
	excludes it at intermediate pitches. The margin is a plane distance;
	the absolute height covers openings both above and below the eye.
	Use the drawn skin, including inverse bind and both bone weights,
	rather than bone origins, which do not bound the surface. */
	tangent = (real)tan(vertical_fov * 0.5);
	margin = 0.03f / (real)cos(vertical_fov * 0.5);
	for (part = 0; part < first_person_legs_part_count; part++)
	{
		struct first_person_legs_part const *entry = &first_person_legs_parts[part];
		long vertex;
		if (entry->definition_index != definition_index) continue;
		for (vertex = 0; vertex < entry->boundary_count; vertex++)
		{
			struct first_person_legs_boundary const *boundary = &entry->boundary[vertex];
			real_point3d a, b, point;
			real x, y, world_x, world_y, current, required;
			matrix4x3_transform_point(&nodes[boundary->nodes[0]], &boundary->local[0], &a);
			matrix4x3_transform_point(&nodes[boundary->nodes[1]], &boundary->local[1], &b);
			point.x = a.x * boundary->weight + b.x * (1.f - boundary->weight);
			point.y = a.y * boundary->weight + b.y * (1.f - boundary->weight);
			point.z = a.z * boundary->weight + b.z * (1.f - boundary->weight);
			x = point.x - pivot->x; y = point.y - pivot->y;
			world_x = pivot->x + cosine * x - sine * y;
			world_y = pivot->y + sine * x + cosine * y;
			current = (eye->x - world_x) * view->i + (eye->y - world_y) * view->j;
			required = (real)fabs(eye->z - point.z) * tangent + margin;
			if (!(required - current > -REAL_MAX && required - current < REAL_MAX))
				return REAL_MAX;
			result = MAX(result, required - current);
		}
	}
	return result;
}
