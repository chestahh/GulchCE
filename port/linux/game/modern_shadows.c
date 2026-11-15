/* Render-only shadow selection and caster collection. The existing feature
   lighting selection is authoritative (including host policy). The prototype
   shadows one light; it neither creates lights nor edits map resources. */
#include "cseries.h"
#include "modern_shadows.h"

#ifndef HALO_GLES
#include <xtl.h>
#include <stdlib.h>
#include <string.h>
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_model_types.h"
#include "rasterizer/rasterizer_lights.h"
#include "scenario/scenario.h"
#include "structures/structure_bsp_definitions.h"
#include "shaders/shader_definitions.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "cache/texture_cache.h"
#include "model_detail.h"
#include "plasma_lighting.h"
#include "plasma_occlusion.h"
#include "physics/breakable_surfaces.h"
#include "objects/light_definitions.h"

extern char const *config_string(char const *name);
extern short custom_edition_part_palette(struct vertex_buffer const *, byte const **);
extern short mcc_cache_part_palette(void const *, byte const **);

enum { SHADOW_PARTS = 2048, SHADOW_LIGHTS = 128 };
/* Validated runtime layouts from models.c; ownership remains with the map. */
struct shadow_geometry { byte reserved[0x24]; struct tag_block parts; };
struct shadow_part
{
	unsigned long flags;
	short shader_index;
	byte reserved[0x1A];
	struct tag_block uncompressed_vertices, compressed_vertices, triangles;
	struct triangle_buffer triangle_buffer;
	struct vertex_buffer vertex_buffer;
};
struct shadow_shader_reference { struct tag_reference shader; short permutation; byte reserved[14]; };
struct shadow_model_shader
{
	struct shader base;
	word flags;
	byte reserved[0x72];
	float u_scale, v_scale;
	struct tag_reference base_map;
};
struct shadow_environment_shader
{
	struct shader base;
	word flags;
	byte reserved[0x5E];
	struct tag_reference base_map;
};
typedef char shadow_part_size[sizeof(struct shadow_part) == 0x68 ? 1 : -1];
typedef char shadow_model_map_offset[offsetof(struct shadow_model_shader, base_map) == 0xA4 ? 1 : -1];
typedef char shadow_environment_map_offset[offsetof(struct shadow_environment_shader, base_map) == 0x88 ? 1 : -1];

static struct
{
	struct vertex_buffer const *vertices;
	void const *triangles;
	int mesh;
	float minimum[3], maximum[3];
} parts[SHADOW_PARTS];
static int part_count;
static struct structure_bsp const *cached_bsp;
static long identities[SHADOW_LIGHTS];
static long selected_identity = NONE, selected_rasterizer = NONE;
static long slots[2] = {NONE, NONE};
static int active, world_stage, receiver_failed;
static float world_extent;
static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

static int setting(void)
{
	char const *value = config_string("display.modern_shadows");
	return !strcmp(value, "hard") ? 1 : !strcmp(value, "soft") ? 2 : 0;
}

void modern_shadows_reset(void)
{
	modern_shadow_dispose();
	memset(parts, 0, sizeof(parts));
	part_count = active = world_stage = receiver_failed = 0;
	cached_bsp = NULL;
	selected_identity = selected_rasterizer = slots[0] = slots[1] = NONE;
}

void modern_shadows_window(void)
{
	int i;
	active = world_stage = 0;
	selected_rasterizer = slots[0] = slots[1] = NONE;
	for (i = 0; i < SHADOW_LIGHTS; i++) identities[i] = NONE;
	modern_shadow_clear_draws();
}

void modern_shadows_submitted(long identity_value, long rasterizer_index)
{
	if (rasterizer_index >= 0 && rasterizer_index < SHADOW_LIGHTS)
		identities[rasterizer_index] = identity_value;
}

void modern_shadows_model_slot(long rasterizer_index, int slot)
{
	if (slot >= 0 && slot < 2) slots[slot] = rasterizer_index;
}

unsigned int modern_shadows_model_mask(void)
{
	if (!active) return 0;
	return (slots[0] == selected_rasterizer ? 1 : 0) | (slots[1] == selected_rasterizer ? 2 : 0);
}

int modern_shadows_selected(long rasterizer_index)
{
	return active && rasterizer_index == selected_rasterizer;
}

void modern_shadows_cancel(void)
{
	int stage = world_stage;
	active = world_stage = 0;
	receiver_failed = 1;
	modern_shadow_clear_draws();
	/* World attenuation was bound before shader selection. Restore its
	   collision mask too; the renderer then refreshes that texture binding. */
	if (stage == 1) plasma_falloff_specular_begin(selected_rasterizer);
	else if (stage == 2) plasma_falloff_begin(selected_rasterizer);
}

void modern_shadows_world_begin(long rasterizer_index, int stage)
{
	world_stage = 0;
	if (!active || rasterizer_index != selected_rasterizer) return;
	world_stage = stage;
	world_extent = rasterizer_lights.lights[rasterizer_index].radius * 2.f;
	if (stage == 1)
		world_extent *= plasma_occlusion_specular_scale(rasterizer_index);
}
void modern_shadows_world_end(void) { world_stage = 0; }
int modern_shadows_world_stage(void) { return world_stage; }
float modern_shadows_world_extent(void) { return world_extent; }

static void matrix_to_gl(real_matrix4x3 const *m, float output[16])
{
	int column, row;
	memcpy(output, identity, sizeof(identity));
	for (column = 0; column < 3; column++)
		for (row = 0; row < 3; row++)
			output[column * 4 + row] = ((float const *)&m->forward)[column * 3 + row] * m->scale;
	memcpy(output + 12, &m->position, 3 * sizeof(float));
}

/* Convert once per immutable map buffer. Skinning stays on the GPU and uses
   the same interpolated pose and CE/MCC palettes as the visible model. */
static int mesh_get(struct vertex_buffer const *vertices, void const *key,
	struct triangle_buffer const *triangles, word const *world_indices, int world_count, int node_count)
{
	struct modern_shadow_vertex *converted = NULL;
	void const *source = NULL;
	word const *indices = world_indices;
	word *list = NULL;
	byte const *palette = NULL, *mcc_palette = NULL;
	short palette_count = 0, mcc_count;
	int i, j, count, size, mesh = 0, part, model;
	for (part = 0; part < part_count; part++)
		if (parts[part].vertices == vertices && parts[part].triangles == key) return part;
	if (part_count == SHADOW_PARTS || vertices->count < 1 || vertices->count > 65535 || !vertices->hardware_format) return NONE;
	model = vertices->type == _rasterizer_vertex_type_model_compressed;
	if (!model && vertices->type != _rasterizer_vertex_type_environment_compressed &&
		vertices->type != _rasterizer_vertex_type_environment_uncompressed) return NONE;
	size = rasterizer_geometry_get_vertex_size(vertices->type);
	if (size < 12 || size > 68) return NONE;
	count = world_count;
	if (triangles)
	{
		if (triangles->count < 1 || triangles->count > 65535 || !triangles->hardware_format ||
			(triangles->type != _triangle_buffer_type_triangles && triangles->type != _triangle_buffer_type_precompiled_strip)) return NONE;
		count = triangles->count * 3;
	}
	if (count < 1 || count > 196605) return NONE;
	if (model)
	{
		palette_count = custom_edition_part_palette(vertices, &palette);
		mcc_count = mcc_cache_part_palette(vertices, &mcc_palette);
		if (mcc_count) { palette_count = mcc_count; palette = mcc_palette; }
		if (palette_count < 0 || palette_count > 64 || (palette_count && !palette)) return NONE;
	}
	/* cseries malloc/free are a matched engine allocator; CRT calloc is not. */
	converted = malloc(vertices->count * sizeof(*converted));
	list = malloc(count * sizeof(*list));
	if (!converted || !list) goto done;
	memset(converted, 0, vertices->count * sizeof(*converted));
	IDirect3DVertexBuffer8_Lock(vertices->hardware_format, 0, vertices->count * size, (BYTE **)&source, D3DLOCK_READONLY);
	if (!source) goto done;
	for (i = 0; i < vertices->count; i++)
	{
		byte const *v = (byte const *)source + i * size;
		memcpy(converted[i].position, v, 3 * sizeof(float));
		converted[i].weight = 1.f;
		if (model)
		{
			struct model_vertex_compressed const *vtx = (void const *)v;
			if (vtx->node_weight < 0) goto done;
			converted[i].weight = vtx->node_weight / 32767.f;
			converted[i].uv[0] = vtx->texcoord.x / 32767.f;
			converted[i].uv[1] = vtx->texcoord.y / 32767.f;
			for (j = 0; j < 2; j++)
			{
				int node = vtx->nodes[j] / 3;
				float weight = j ? 1.f - converted[i].weight : converted[i].weight;
				if (weight == 0.f) node = 0;
				else
				{
					if (vtx->nodes[j] % 3) goto done;
					if (palette_count)
					{
						if (node >= palette_count) goto done;
						node = palette[node];
					}
					if (node >= node_count) goto done;
				}
				converted[i].nodes[j] = (float)node;
			}
		}
		else memcpy(converted[i].uv, v + size - 8, 8);
		for (j = 0; j < 3; j++)
		{
			float p = converted[i].position[j];
			if (!(p > -REAL_MAX && p < REAL_MAX)) goto done;
			if (!i || p < parts[part_count].minimum[j]) parts[part_count].minimum[j] = p;
			if (!i || p > parts[part_count].maximum[j]) parts[part_count].maximum[j] = p;
		}
	}
	if (triangles)
	{
		int input_count = triangles->type == _triangle_buffer_type_triangles ? count : triangles->count + 2;
		IDirect3DIndexBuffer8_Lock(triangles->hardware_format, 0, input_count * sizeof(word), (BYTE **)&indices, D3DLOCK_READONLY);
		if (!indices) goto done;
		for (i = 0; i < triangles->count; i++)
		{
			int first = triangles->type == _triangle_buffer_type_triangles ? i * 3 : i;
			for (j = 0; j < 3; j++) list[i * 3 + j] = indices[first + j];
		}
	}
	else memcpy(list, indices, count * sizeof(word));
	mesh = modern_shadow_mesh_create(converted, vertices->count, list, count);
done:
	if (source) IDirect3DVertexBuffer8_Unlock(vertices->hardware_format);
	if (triangles && indices) IDirect3DIndexBuffer8_Unlock(triangles->hardware_format);
	if (converted) free(converted);
	if (list) free(list);
	if (!mesh) return NONE;
	parts[part_count].vertices = vertices;
	parts[part_count].triangles = key;
	parts[part_count].mesh = mesh;
	return part_count++;
}

/* Alpha testing follows the material's base map. Transparent shells, effects
   and decals do not become opaque blockers. No material name heuristics. */
static int material(struct shader const *shader, short permutation, void const **alpha, float scale[2])
{
	long bitmap_index = NONE;
	int tested = 0;
	*alpha = NULL;
	if (shader->base.type == 4)
	{
		struct shadow_model_shader const *s = (void const *)shader;
		if (s->flags & (1 << 3)) return 0;
		tested = !(s->flags & (1 << 2));
		bitmap_index = s->base_map.index;
		scale[0] *= s->u_scale;
		scale[1] *= s->v_scale;
	}
	else if (shader->base.type == 3)
	{
		struct shadow_environment_shader const *s = (void const *)shader;
		tested = s->flags & 1;
		bitmap_index = s->base_map.index;
	}
	else return 0;
	if (tested && bitmap_index != NONE)
	{
		struct bitmap_group *group = bitmap_group_get(bitmap_index);
		struct bitmap_data *bitmap;
		if (group->bitmaps.count < 1) return -1;
		bitmap = bitmap_group_try_and_get_bitmap(bitmap_index, MAX(0, permutation) % group->bitmaps.count);
		if (!bitmap || bitmap->type != 0) return -1;
		*alpha = _texture_cache_bitmap_get_hardware_format(bitmap, TRUE, TRUE);
		if (!*alpha) return -1;
	}
	return 1;
}

static int world_casters(struct structure_bsp const *bsp, real_point3d const *origin, real radius)
{
	int lightmap, index;
	for (lightmap = 0; lightmap < bsp->lightmaps.count; lightmap++)
	{
		struct structure_lightmap const *lm = TAG_BLOCK_GET_ELEMENT(&bsp->lightmaps, lightmap, struct structure_lightmap);
		for (index = 0; index < lm->materials.count; index++)
		{
			struct structure_material const *m = TAG_BLOCK_GET_ELEMENT(&lm->materials, index, struct structure_material);
			struct structure_surface const *surfaces;
			void const *alpha;
			float scale[2] = {1,1}, distance = 0;
			int part, axis, type;
			if (m->surface_count == 0 || m->shader.index == NONE) continue;
			if (m->breakable_surface_index != NONE && !breakable_surface_extant(m->breakable_surface_index)) continue;
			type = material(shader_definition_get(m->shader.index), m->permutation_index, &alpha, scale);
			if (!type) continue;
			if (type < 0 || m->surface_count < 0 || m->surface_count > 65535 || m->first_surface_index < 0 ||
				m->first_surface_index > bsp->surfaces.count - m->surface_count) return 0;
			surfaces = (struct structure_surface const *)bsp->surfaces.address + m->first_surface_index;
			part = mesh_get(&m->vertices, m, NULL, surfaces->vertex_indices, m->surface_count * 3, 1);
			if (part == NONE) return 0;
			for (axis = 0; axis < 3; axis++)
			{
				float p = ((float const *)origin)[axis];
				float d = MAX(parts[part].minimum[axis] - p, MAX(p - parts[part].maximum[axis], 0.f));
				distance += d * d;
			}
			if (distance > radius * radius) continue;
			if (!modern_shadow_draw(parts[part].mesh, identity, 1, alpha, scale)) return 0;
		}
	}
	return 1;
}

static int object_casters(real_point3d const *origin, real radius)
{
	struct object_iterator iterator;
	struct object_datum *object;
	object_iterator_new(&iterator, _object_mask_all, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		struct object_definition const *definition;
		struct model *model;
		real_matrix4x3 const *nodes;
		float matrices[64 * 16], range;
		int node, region;
		if (object->object.flags & FLAG(_object_invisible_bit)) continue;
		range = radius + object->object.bounding_sphere_radius;
		if (distance_squared3d(origin, &object->object.bounding_sphere_center) > range * range) continue;
		definition = object_definition_get(object->definition_index);
		if (definition->object.model.index == NONE) continue;
		model = model_definition_get(definition->object.model.index);
		if (model->nodes.count < 1) continue;
		if (model->nodes.count > 64 || object->object.node_matrices.size / sizeof(real_matrix4x3) < (unsigned long)model->nodes.count) return 0;
		nodes = object_get_node_matrices(iterator.index);
		if (!nodes) return 0;
		for (node = 0; node < model->nodes.count; node++)
		{
			struct model_node const *n = TAG_BLOCK_GET_ELEMENT(&model->nodes, node, struct model_node);
			real_matrix4x3 relative;
			matrix4x3_multiply(&nodes[node], &n->runtime_default_inverse_matrix, &relative);
			matrix_to_gl(&relative, matrices + node * 16);
		}
		for (region = 0; region < MIN(model->regions.count, MAXIMUM_REGIONS_PER_OBJECT); region++)
		{
			struct model_region const *r = TAG_BLOCK_GET_ELEMENT(&model->regions, region, struct model_region);
			struct model_region_permutation const *permutation;
			struct shadow_geometry const *geometry;
			int index = object->object.region_permutations[region], geometry_index, part_index;
			if (index == NONE) continue;
			if (!VALID_INDEX(index, r->permutations.count)) return 0;
			permutation = TAG_BLOCK_GET_ELEMENT(&r->permutations, index, struct model_region_permutation);
			geometry_index = model_detail_geometry_index(model, permutation, 4, _model_detail_rules_outside);
			if (geometry_index == NONE) continue;
			if (!VALID_INDEX(geometry_index, model->geometries.count)) return 0;
			geometry = TAG_BLOCK_GET_ELEMENT(&model->geometries, geometry_index, struct shadow_geometry);
			for (part_index = 0; part_index < geometry->parts.count; part_index++)
			{
				struct shadow_part const *p = TAG_BLOCK_GET_ELEMENT(&geometry->parts, part_index, struct shadow_part);
				struct shadow_shader_reference const *reference;
				void const *alpha;
				float scale[2] = {model->base_map_scale.i, model->base_map_scale.j};
				int part, type;
				if ((p->flags & 1) || !p->triangle_buffer.count) continue;
				if (!VALID_INDEX(p->shader_index, model->shaders.count)) return 0;
				reference = TAG_BLOCK_GET_ELEMENT(&model->shaders, p->shader_index, struct shadow_shader_reference);
				if (reference->shader.index == NONE) continue;
				type = material(shader_definition_get(reference->shader.index), reference->permutation, &alpha, scale);
				if (!type) continue;
				if (type < 0) return 0;
				part = mesh_get(&p->vertex_buffer, &p->triangle_buffer, &p->triangle_buffer, NULL, 0, model->nodes.count);
				if (part == NONE || !modern_shadow_draw(parts[part].mesh, matrices, model->nodes.count, alpha, scale)) return 0;
			}
		}
	}
	return 1;
}

void modern_shadows_prepare(int primary)
{
	struct structure_bsp const *bsp;
	struct rasterizer_light_submit_parameters const *light;
	float best = 0, previous_score = 0, radius;
	long best_index = NONE, previous = NONE, index;
	int mode = setting(), complete;
	active = world_stage = 0;
	if (!mode) { if (cached_bsp) modern_shadows_reset(); return; }
	if (!primary) return;
	bsp = global_structure_bsp_get();
	if (bsp != cached_bsp) { modern_shadows_reset(); cached_bsp = bsp; }
	if (!bsp || receiver_failed) return;
	for (index = 0; index < MIN(rasterizer_lights.light_count, SHADOW_LIGHTS); index++)
	{
		float score, brightness;
		if (identities[index] == NONE || !plasma_lighting_selected(index)) continue;
		light = &rasterizer_lights.lights[index];
		if (!(light->radius > 0.01f && light->radius < 1000.f)) continue;
		brightness = MAX(light->color.red, MAX(light->color.green, light->color.blue));
		score = brightness * light->radius * light->radius / (1.f + distance_squared3d(&render.camera.position, &light->position));
		if (score > best) { best = score; best_index = index; }
		if (identities[index] == selected_identity) { previous = index; previous_score = score; }
	}
	/* Keep the current identity while it remains competitive. Never reuse a
   shadow from a recycled light index or a previous render window. */
	if (previous != NONE && previous_score >= best * 0.8f) best_index = previous;
	if (best_index == NONE) { selected_identity = selected_rasterizer = NONE; return; }
	selected_rasterizer = best_index;
	selected_identity = identities[best_index];
	light = &rasterizer_lights.lights[best_index];
	/* Specular can reach beyond the diffuse sphere; both passes sample the
	   same map and need blockers throughout their full authored extent. */
	radius = light->radius * MAX(1.f, plasma_occlusion_specular_scale(best_index));
	if (!modern_shadow_begin((float const *)&light->position, radius, mode == 2)) return;
	complete = world_casters(bsp, &light->position, radius) && object_casters(&light->position, radius);
	active = modern_shadow_end() && complete;
	if (!active) modern_shadow_clear_draws();
}
#else
void modern_shadows_prepare(int primary) {}
void modern_shadows_reset(void) {}
void modern_shadows_window(void) {}
void modern_shadows_submitted(long identity, long index) {}
void modern_shadows_model_slot(long index, int slot) {}
void modern_shadows_world_begin(long index, int stage) {}
void modern_shadows_world_end(void) {}
unsigned int modern_shadows_model_mask(void) { return 0; }
int modern_shadows_selected(long index) { return 0; }
void modern_shadows_cancel(void) {}
int modern_shadows_world_stage(void) { return 0; }
float modern_shadows_world_extent(void) { return 0; }
#endif
