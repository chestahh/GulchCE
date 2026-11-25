/* Render-only shadow selection and caster collection. The existing feature
   lighting selection is authoritative (including host policy). This feature
   shadows a bounded light set; it neither creates lights nor edits map resources. */
#include "cseries.h"
#include "modern_shadows.h"

#ifndef HALO_GLES
#include <xtl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_lights.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "units/units.h"
#include "rasterizer/rasterizer_geometry.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_model_types.h"
#include "rasterizer/rasterizer_lights.h"
#include "scenario/scenario.h"
#include "structures/structure_bsp_definitions.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"
#include "cache/texture_cache.h"
#include "model_detail.h"
#include "plasma_lighting.h"
#include "plasma_occlusion.h"
#include "muzzle_lighting.h"
#include "physics/breakable_surfaces.h"
#include "objects/light_definitions.h"
#include "render/render.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/progress_bar.h"
#include "rasterizer/rasterizer_console_vars.h"

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
	byte reserved_b4[0x48];
	struct shader_texture_animation animation;
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
typedef char shadow_model_animation_offset[offsetof(struct shadow_model_shader, animation) == 0xFC ? 1 : -1];
typedef char shadow_environment_map_offset[offsetof(struct shadow_environment_shader, base_map) == 0x88 ? 1 : -1];

static struct
{
	struct vertex_buffer const *vertices;
	void const *triangles;
	int mesh;
	float minimum[3], maximum[3];
} parts[SHADOW_PARTS];
/* Zero means empty. The map-lifetime cache holds at most half the buckets. */
enum { MESH_BUCKETS = 2 * SHADOW_PARTS };
static unsigned short mesh_index[MESH_BUCKETS];
static int patches_ready;
static unsigned char *sun_surface_sky;
static int sun_count;
static short prepared_sky = NONE;
enum { SUN_CUTOUTS = 64 };
static struct
{
    struct structure_material const *material;
    const void *texture;
    float uv[8];
} sun_cutouts[SUN_CUTOUTS];
static int sun_cutout_count;
static int part_count;
static int map_mode, prepare_pending;
static struct structure_bsp const *cached_bsp;
static long identities[SHADOW_LIGHTS];
static struct shadow_selection
{
	long identity, rasterizer, challenger, unshadowed_owner, collision_owner;
	float since, time;
} selections[MODERN_SHADOW_LIGHTS];
static long flashlight_index = NONE;
static int flash_world;
static int world_map;
static long slots[2] = {NONE, NONE};
static int active, world_stage, receiver_failed;
static int view_receiver;
static long receiver_object = NONE;
/* First-person draw identifiers are not object datums. Use the view pose owner. */
static long view_unit = NONE;
static float world_extent;
static const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

static int setting(void)
{
	return map_mode;
}

void modern_shadows_map_loaded(int gameplay)
{
	char const *value = gameplay ? config_string("display.modern_shadows") : "off";
	map_mode = !strcmp(value, "hard") ? 1 : !strcmp(value, "soft") ? 2 : 0;
	prepare_pending = map_mode != 0;
}

int modern_shadows_enabled(void) { return setting() != 0; }
int modern_shadows_hard_enabled(void) { return setting() == 1; }
void modern_shadows_environment_receivers(int primary)
{
    modern_environment_receivers(primary && setting() != 0 && cached_bsp != NULL);
}

static void forget_shadow_geometry(void)
{
	int i;
	if (sun_surface_sky) free(sun_surface_sky);
	sun_surface_sky = NULL;
	patches_ready = 0;
	sun_cutout_count = sun_count = 0;
	prepared_sky = NONE;
	memset(parts, 0, sizeof(parts));
	memset(mesh_index, 0, sizeof(mesh_index));
	part_count = active = world_stage = receiver_failed = 0;
	view_receiver = 0;
	receiver_object = view_unit = NONE;
	cached_bsp = NULL;
	slots[0] = slots[1] = NONE;
	memset(selections, 0, sizeof(selections));
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
		selections[i].identity = selections[i].rasterizer = selections[i].challenger = selections[i].unshadowed_owner = NONE;
}

/* Sun and fixture depth include the current breakable surfaces. A map can
   contain glass without changing it on every frame. Invalidate all three
   static images together, even when a fixture is temporarily out of view. */
static void check_static_breakables(struct structure_bsp const *bsp)
{
	static byte previous[MAXIMUM_BREAKABLE_SURFACES_PER_MAP / 8];
	const byte *flags;
	if (!bsp->breakable_surfaces.count) return;
	flags = breakable_surface_flags_get();
	if (memcmp(previous, flags, sizeof(previous)))
	{
		modern_environment_invalidate_static();
		memcpy(previous, flags, sizeof(previous));
	}
}

void modern_shadows_reset(void)
{
	modern_shadow_dispose();
	forget_shadow_geometry();
}

void modern_shadows_window(void)
{
	int i;
	active = world_stage = 0;
	view_receiver = 0;
	receiver_object = view_unit = NONE;
	slots[0] = slots[1] = NONE;
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++) selections[i].rasterizer = selections[i].unshadowed_owner = NONE;
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

static int map_for(long rasterizer)
{
	int i;
	if (rasterizer == NONE) return NONE;
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
		if ((active & (1 << i)) && selections[i].rasterizer == rasterizer) return i;
	return NONE;
}

/* A shield light exempts its emitting biped. Include only its
   equipped weapon. Ghost engine lights include seated occupants and their gear. */
static int unshadowed_object(long owner, long object)
{
	int depth;
	if (owner == NONE || object == NONE) return FALSE;
	if (object == owner) return TRUE;
	for (depth = 0; depth < MAXIMUM_OBJECTS_PER_MAP; depth++)
	{
		struct object_datum *datum = object_try_and_get(object);
		struct unit_datum *unit;
		long parent;
		if (!datum || (parent = datum->object.parent_object_index) == NONE) return FALSE;
		if (parent == owner)
		{
			if (depth == 0 && datum->object.type == _object_type_weapon)
			{
				unit = unit_try_and_get(owner);
				if (unit && unit->object.type == _object_type_biped)
				{
					short slot = unit->unit.current_weapon_index;
					return slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT &&
						unit_inventory_get_weapon(owner, slot) == object;
				}
			}
			unit = unit_try_and_get(object);
			return unit && unit->unit.parent_seat_index != NONE &&
				object_try_and_get_and_verify_type(owner, _object_mask_vehicle) != NULL;
		}
		object = parent;
	}
	return FALSE;
}

/* Keep the map selected globally: other receivers use it, and the ordinary
   collision mask must not reintroduce the owner's self-shadow on this draw. */
static int model_map(int slot)
{
	int map = map_for(slots[slot]);
	return map != NONE && receiver_object != NONE &&
		unshadowed_object(selections[map].unshadowed_owner, receiver_object) ? NONE : map;
}

unsigned int modern_shadows_model_mask(void)
{
	return (model_map(0) != NONE ? 1 : 0) | (model_map(1) != NONE ? 2 : 0);
}

/* World attenuation binds one map; each of the two model light slots can
   independently name any map. Slot identities never follow array order. */
int modern_shadows_map(int slot)
{
	if (world_stage) return slot == 0 ? world_map : NONE;
	return slot >= 0 && slot < 2 ? model_map(slot) : NONE;
}

void modern_shadows_receiver(int first_person, long object)
{
	view_receiver = active && first_person;
	receiver_object = active ? (first_person ? view_unit : object) : NONE;
}
int modern_shadows_view_receiver(void) { return view_receiver && !world_stage; }
int modern_shadows_selected(long rasterizer_index) { return map_for(rasterizer_index) != NONE; }
/* The owner's collision field complements this map; other lights keep their
   complete rendered casters and ordinary collision-replacement behaviour. */
long modern_shadows_collision_owner(long rasterizer_index)
{
	int slot = map_for(rasterizer_index);
	return slot != NONE ? selections[slot].collision_owner : NONE;
}

void modern_shadows_cancel(void)
{
	int stage = world_stage;
	flash_world = 0;
	long rasterizer = stage ? selections[world_map].rasterizer : NONE;
	active = world_stage = 0;
	receiver_failed = 1;
	modern_shadow_clear_draws();
	/* World attenuation was bound before shader selection. Restore its
	   collision mask too; the renderer then refreshes that texture binding. */
	if (stage == 1) plasma_falloff_specular_begin(rasterizer);
	else if (stage == 2) plasma_falloff_begin(rasterizer);
}

void modern_shadows_world_begin(long rasterizer_index, int stage)
{
	world_stage = 0;
	world_map = map_for(rasterizer_index);
	if (world_map == NONE) return;
	world_stage = stage;
	world_extent = rasterizer_lights.lights[rasterizer_index].radius * 2.f;
	if (stage == 1)
		world_extent *= plasma_occlusion_specular_scale(rasterizer_index);
}
void modern_shadows_world_end(void) { flash_world = world_stage = 0; }
int modern_shadows_world_stage(void) { return flash_world ? 4 : world_stage; }
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
static unsigned mesh_bucket(struct vertex_buffer const *vertices, void const *triangles)
{
	unsigned hash = (unsigned)((uintptr_t)vertices >> 2) * 2654435761u + (unsigned)((uintptr_t)triangles >> 2);
	unsigned bucket = (hash ^ (hash >> 16)) & (MESH_BUCKETS - 1);
	while (mesh_index[bucket])
	{
		const int part = mesh_index[bucket] - 1;
		if (parts[part].vertices == vertices && parts[part].triangles == triangles) break;
		bucket = (bucket + 1) & (MESH_BUCKETS - 1);
	}
	return bucket;
}

static int mesh_get(struct vertex_buffer const *vertices, void const *key,
	struct triangle_buffer const *triangles, word const *world_indices, int world_count, int node_count)
{
	struct modern_shadow_vertex *converted = NULL;
	void const *source = NULL;
	word const *indices = world_indices;
	word *list = NULL;
	byte const *palette = NULL, *mcc_palette = NULL;
	short palette_count = 0, mcc_count;
	int i, j, count, size, mesh = 0, model;
	unsigned bucket = mesh_bucket(vertices, key);
	if (mesh_index[bucket]) return mesh_index[bucket] - 1;
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
	mesh = modern_shadow_mesh_create(converted, vertices->count, list, count, triangles != NULL);
done:
	if (source) IDirect3DVertexBuffer8_Unlock(vertices->hardware_format);
	if (triangles && indices) IDirect3DIndexBuffer8_Unlock(triangles->hardware_format);
	if (converted) free(converted);
	if (list) free(list);
	if (!mesh) return NONE;
	parts[part_count].vertices = vertices;
	parts[part_count].triangles = key;
	parts[part_count].mesh = mesh;
	mesh_index[bucket] = (unsigned short)(part_count + 1);
	return part_count++;
}

/* Alpha testing follows the material's base map. Transparent shells, effects
   and decals do not become opaque blockers. No material name heuristics. */
static int material(struct shader const *shader, short permutation, void const **alpha,
	float const scale[2], real const *values, float uv[8])
{
	long bitmap_index = NONE;
	int tested = 0;
	memset(uv, 0, 8 * sizeof(float));
	uv[0] = scale[0]; uv[5] = scale[1];
	*alpha = NULL;
	if (shader->base.type == 4)
	{
		struct shadow_model_shader const *s = (void const *)shader;
		if (s->flags & (1 << 3)) return 0;
		tested = !(s->flags & (1 << 2));
		bitmap_index = s->base_map.index;
		if (tested)
		{
			struct render_animation animation;
			animation.colors = NULL;
			animation.values = values;
			shader_texture_animation_evaluate(&s->animation, values ? &animation : NULL,
				scale[0] * s->u_scale, scale[1] * s->v_scale, 0, 0, 0,
				global_frame_parameters.game_time_sec, (real_vector4d *)uv, (real_vector4d *)(uv + 4));
		}
	}
	else if (shader->base.type == 3)
	{
		struct shadow_environment_shader const *s = (void const *)shader;
		tested = s->flags & 1;
		bitmap_index = s->base_map.index;
		if (tested) shader_environment_texture_animation_evaluate(shader,
			global_frame_parameters.game_time_sec, &uv[3], &uv[7]);
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

/* Unit normals point into the spotlight frustum. Use a box's support
   vertex to reject a complete BSP batch before submitting its triangles. */
static float spot_planes[6][4];
static int clipping_spot;
static void spot_clip_planes(const float frame[16])
{
    for (int i=0;i<6;i++)
    {
        float *p=spot_planes[i];
        for (int j=0;j<4;j++)
        {
            const float *col=frame+4*j;
            if (i<4) p[j]=col[3]+(i&1 ? -col[i/2] : col[i/2]);
            else p[j]=i==4 ? col[2] : col[3]-col[2];
        }
        float inv=1.f/sqrtf(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
        for (int k=0;k<4;k++) p[k]*=inv;
    }
}
static int spot_box_visible(const float low[3],const float high[3])
{
    for (int i=0;i<6;i++)
    {
        const float *p=spot_planes[i];
        float d=p[3];
        for (int j=0;j<3;j++) d+=p[j]*(p[j]>=0.f?high[j]:low[j]);
        if (d<-.001f) return 0;
    }
    return 1;
}
static int spot_sphere_visible(real_point3d const *c,float radius)
{
    for (int i=0;i<6;i++)
    {
        const float *p=spot_planes[i];
        if (p[0]*c->x+p[1]*c->y+p[2]*c->z+p[3] < -radius-.001f) return 0;
    }
    return 1;
}

static int world_casters_clipped(struct structure_bsp const *bsp, real_point3d const *origin, real radius, const float *sun_frame)
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
			float scale[2] = {1,1}, uv[8], distance = 0;
			int part, axis, type;
			if (m->surface_count == 0 || m->shader.index == NONE) continue;
			if (m->breakable_surface_index != NONE && !breakable_surface_extant(m->breakable_surface_index)) continue;
			type = material(shader_definition_get(m->shader.index), m->permutation_index, &alpha, scale, NULL, uv);
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
			if (sun_frame)
			{
				int outside = 0;
				for (axis = 0; axis < 2; axis++)
				{
					const float *basis = sun_frame + 4 * axis;
					float offset = 0.f, extent = 0.f;
					for (int k = 0; k < 3; k++)
					{
						float lo = parts[part].minimum[k], hi = parts[part].maximum[k];
						offset += basis[k] * ((lo + hi) * .5f - sun_frame[12+k]);
						extent += fabsf(basis[k]) * (hi-lo) * .5f;
					}
					if (fabsf(offset) > radius + extent) outside = 1;
				}
				if (outside) continue;
			}
			else if (distance > radius * radius) continue;
			if (clipping_spot && !spot_box_visible(parts[part].minimum,parts[part].maximum)) continue;
			if (!modern_shadow_draw(parts[part].mesh, identity, 1, alpha, uv, MODERN_SHADOW_BOTH)) return 0;
			if (sun_frame)
			{
				modern_environment_sun_surfaces(sun_surface_sky + m->first_surface_index);
				if (alpha && sun_cutout_count < SUN_CUTOUTS)
				{
					sun_cutouts[sun_cutout_count].material = m;
					sun_cutouts[sun_cutout_count].texture = alpha;
					memcpy(sun_cutouts[sun_cutout_count++].uv,uv,sizeof(uv));
				}
				else if (alpha) sun_cutout_count = SUN_CUTOUTS + 1;
			}
		}
	}
	return 1;
}

static int world_casters(struct structure_bsp const *bsp, real_point3d const *origin, real radius)
{
	return world_casters_clipped(bsp, origin, radius, NULL);
}

/* Both model sources share conversion, skinning and material evaluation. */
static int model_caster(struct model const *model, real_matrix4x3 const *nodes,
	byte const *permutations, short forced_permutation, real const *values, int views)
{
	float matrices[64 * 16];
	int node, region;
	if (model->nodes.count < 1) return 1;
	if (model->nodes.count > 64 || !nodes) return 0;
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
		int index = permutations ? (signed char)permutations[region] : 0, geometry_index, part_index;
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
			float scale[2] = {model->base_map_scale.i, model->base_map_scale.j}, uv[8];
			int part, type;
			if ((p->flags & 1) || !p->triangle_buffer.count) continue;
			if (!VALID_INDEX(p->shader_index, model->shaders.count)) return 0;
			reference = TAG_BLOCK_GET_ELEMENT(&model->shaders, p->shader_index, struct shadow_shader_reference);
			if (reference->shader.index == NONE) continue;
			type = material(shader_definition_get(reference->shader.index), forced_permutation ? forced_permutation : reference->permutation, &alpha, scale, values, uv);
			if (!type) continue;
			if (type < 0) return 0;
			part = mesh_get(&p->vertex_buffer, &p->triangle_buffer, &p->triangle_buffer, NULL, 0, model->nodes.count);
			if (part == NONE || !modern_shadow_draw(parts[part].mesh, matrices, model->nodes.count, alpha, uv, views)) return 0;
		}
	}
	return 1;
}

static int viewmodel_casters(long owners[2], long excluded_caster, long unshadowed_owner)
{
	int hands;
	owners[0] = owners[1] = NONE;
	for (hands = 0; hands < 2; hands++)
	{
		real_matrix4x3 nodes[64];
		real values[NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS];
		long model_index, owner;
		int result = modern_shadows_viewmodel(hands, &model_index, &owner, nodes);
		if (result < 0) return 0;
		owners[hands] = owner;
		if (!result || owner == excluded_caster || unshadowed_object(unshadowed_owner, owner)) continue;
		object_function_values_for_render(owner, values);
		if (!model_caster(model_definition_get(model_index), nodes, NULL, 0, values, MODERN_SHADOW_VIEW)) return 0;
	}
	return 1;
}

static int object_casters(real_point3d const *origin, real radius, long const owners[2], long unshadowed_owner, long excluded_caster, long emitter)
{
	struct object_iterator iterator;
	struct object_datum *object;
	object_iterator_new(&iterator, _object_mask_all, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		struct object_definition const *definition;
		struct model *model;
		real values[NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS];
		float range;
		int views;
		if ((object->object.flags & FLAG(_object_invisible_bit)) || iterator.index == excluded_caster || iterator.index == emitter ||
			unshadowed_object(unshadowed_owner, iterator.index)) continue;
        real_point3d center;
        real extent;
        object_get_render_bounding_sphere(iterator.index,&center,&extent);
        render_interpolation_object_point(iterator.index,&center);
        range = radius + MAX(extent,object->object.bounding_sphere_radius);
        if (distance_squared3d(origin,&center) > range*range) continue;
        if (clipping_spot && !spot_sphere_visible(&center,extent)) continue;
		definition = object_definition_get(object->definition_index);
		if (definition->object.model.index == NONE) continue;
		model = model_definition_get(definition->object.model.index);
		if (object->object.node_matrices.size / sizeof(real_matrix4x3) < (unsigned long)model->nodes.count) return 0;
		/* Preserve the complete body and held weapon in world shadows, but
		   never project their hidden geometry onto their viewmodels. */
		views = iterator.index == owners[0] || iterator.index == owners[1] ? MODERN_SHADOW_WORLD : MODERN_SHADOW_BOTH;
		object_function_values_for_render(iterator.index, values);
		if (!model_caster(model, object_get_node_matrices(iterator.index), object->object.region_permutations,
			object->object.forced_shader_permutation_index, values, views)) return 0;
	}
	return 1;
}

static long select_light(struct shadow_selection *selection, long best, long previous, float best_score, float previous_score)
{
	float now = global_frame_parameters.game_time_sec;
	if (now < selection->time) selection->challenger = NONE;
	selection->time = now;
	if (best == NONE) { selection->challenger = NONE; return NONE; }
	/* A modest challenger must remain stronger for 100 ms, independent of
	   frame rate. Bright flashes respond immediately. Invalid or forbidden
	   previous lights never retain a slot while waiting for a challenger. */
	if (previous != NONE && previous != best && previous_score < best_score * 0.8f &&
		previous_score > best_score * 0.5f)
	{
		if (selection->challenger != identities[best])
		{
			selection->challenger = identities[best];
			selection->since = now;
		}
		if (now - selection->since < 0.1f) return previous;
	}
	else if (previous != NONE && previous_score >= best_score * 0.8f)
	{
		selection->challenger = NONE;
		return previous;
	}
	selection->challenger = NONE;
	return best;
}

static void select_lights(void)
{
	float scores[SHADOW_LIGHTS] = {0}, retained[MODERN_SHADOW_LIGHTS] = {0};
	long index;
	int i, order, ordered[MODERN_SHADOW_LIGHTS];
	int authored = config_boolean("display.environment_fixtures") && game_engine_allow_dynamic_lighting();
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++) selections[i].rasterizer = selections[i].unshadowed_owner = NONE;
	for (index = 0; index < MIN(rasterizer_lights.light_count, SHADOW_LIGHTS); index++)
	{
		struct rasterizer_light_submit_parameters const *light = &rasterizer_lights.lights[index];
		float brightness, score;
		if (identities[index] == NONE || (!plasma_lighting_selected(index) &&
			!(authored && light_port_authored_dynamic(identities[index])))) continue;
		if (!(light->radius > 0.01f && light->radius < 1000.f)) continue;
		brightness = MAX(light->color.red, MAX(light->color.green, light->color.blue));
		score = brightness * light->radius * light->radius / (1.f + distance_squared3d(&render.camera.position, &light->position));
		if (!(score > 0)) continue;
		scores[index] = score;
		for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
			if (identities[index] == selections[i].identity) { selections[i].rasterizer = index; retained[i] = score; }
	}
	/* Fill vacancies / replace the weaker incumbent first, without moving a
	   surviving light between maps. A pending challenger cannot own two slots. */
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
	{
		int j = i;
		while (j && retained[ordered[j - 1]] > retained[i])
		{
			ordered[j] = ordered[j - 1];
			j--;
		}
		ordered[j] = i;
	}
	for (order = 0; order < MODERN_SHADOW_LIGHTS; order++)
	{
		int slot = ordered[order];
		float best = 0;
		long best_index = NONE;
		for (index = 0; index < MIN(rasterizer_lights.light_count, SHADOW_LIGHTS); index++)
		{
			int other;
			for (other = 0; other < MODERN_SHADOW_LIGHTS; other++)
				if (other != slot && selections[other].rasterizer != NONE &&
					identities[index] == selections[other].identity) break;
			if (other < MODERN_SHADOW_LIGHTS) continue;
			if (scores[index] > best) { best = scores[index]; best_index = index; }
		}
		best_index = select_light(&selections[slot], best_index, selections[slot].rasterizer, best, retained[slot]);
		selections[slot].rasterizer = best_index;
		selections[slot].identity = best_index != NONE ? identities[best_index] : NONE;
	}
	/* A published flashlight uses a single projector inside the local budget.
	   Keep the displaced light's identity for the next view and its lighting
	   contribution in the stock renderer. */
	if ((modern_spot_mask() & 4) && !config_boolean("display.flashlight_extra_slot"))
	{
		int weakest = 0;
		for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
		{
			if (selections[i].rasterizer == NONE) return;
			if (scores[selections[i].rasterizer] < scores[selections[weakest].rasterizer]) weakest = i;
		}
		selections[weakest].rasterizer = NONE;
	}
}

/* Environmental maps reuse the validated caster conversion, never local-light
   slots. Traversal is bounded even for malformed attachment chains. */
static int environmental_casters(long object_index, int *remaining)
{
    struct object_datum *object;
    struct object_definition const *definition;
    real values[NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS];
    long child;
    if (*remaining <= 0 || !(object = object_try_and_get(object_index))) return 0;
    (*remaining)--;
    definition = object_definition_get(object->definition_index);
    if (!(object->object.flags & FLAG(_object_invisible_bit)) && definition->object.model.index != NONE)
    {
        struct model const *model = model_definition_get(definition->object.model.index);
        if (model->nodes.count < 0 || model->nodes.count > (int)(object->object.node_matrices.size / sizeof(real_matrix4x3))) return 0;
        object_function_values_for_render(object_index, values);
        if (!model_caster(model, object_get_node_matrices(object_index),
            object->object.region_permutations, object->object.forced_shader_permutation_index, values, MODERN_SHADOW_WORLD)) return 0;
    }
    child = object->object.first_child_object_index;
    while (child != NONE)
    {
        if (!environmental_casters(child, remaining)) return 0;
        child = object_get(child)->object.next_object_index;
    }
    return 1;
}

/* Collision spheres need not enclose rendered hulls or held weapons. Use the
   render owner's bounds for every member of the same bounded caster family. */
static int environmental_family_bounds(long index, const real_point3d *center, real *radius, int *remaining, int casters_only)
{
    struct object_datum *object;
    real_point3d point;
    real extent;
    long child;
    if (*remaining <= 0 || !(object = object_try_and_get(index))) return 0;
    (*remaining)--;
    /* Logical attachments such as the Warthog gun have no caster geometry.
       Their cached centre may be in attachment space. Visit their children. */
    if (!casters_only || (!(object->object.flags & FLAG(_object_invisible_bit)) &&
        object_definition_get(object->definition_index)->object.model.index != NONE))
    {
        object_get_render_bounding_sphere(index, &point, &extent);
        render_interpolation_object_point(index, &point);
        if (!(extent >= 0 && extent < REAL_MAX)) return 0;
        extent += distance3d(center, &point);
        if (!(extent < REAL_MAX)) return 0;
        if (extent > *radius) *radius = extent;
    }
    child = object->object.first_child_object_index;
    while (child != NONE)
    {
        if (!environmental_family_bounds(child, center, radius, remaining, casters_only)) return 0;
        child = object_get(child)->object.next_object_index;
    }
    return 1;
}

#include "modern_sun.inl"
#include "modern_spots.inl"

/* Object placement has finished. A restart or BSP switch can initialise
   game state again, but must never start a second material preparation. */
int modern_shadows_prepare_loaded_map(void)
{
    int sunlight, fixtures;
    if (!prepare_pending) return FALSE;
    prepare_pending = FALSE;
    if (!game_engine_allow_dynamic_lighting()) return FALSE;
    cached_bsp = global_structure_bsp_get();
    if (!cached_bsp) return FALSE;
    /* A cached map can bypass the copy progress loop entirely. Leave the
       stock loading frame on screen during GPU setup and material draws. */
    if (!progress_bar_is_active())
    {
        progress_bar_eachframe();
        progress_bar_begin(TRUE);
        progress_bar_present_standalone(.95f);
        progress_bar_end();
    }
    sunlight = config_boolean("display.environment_sun");
    fixtures = config_boolean("display.environment_fixtures");
    if (sunlight) sun_classify_surfaces();
    if (fixtures) prepare_fixture_patches();
    modern_shadow_preload(sunlight, fixtures, config_boolean("display.flashlight_shadows"));
    return TRUE;
}

/* Draw-packet lighting is authoritative, including queued material passes.
   Shadow only the two distant terms; ambient/local illumination is untouched. */
static void environment_model_family(const struct rasterizer_model_begin_parameters *parameters, int first_person)
{
    static long previous = NONE;
    static float previous_frames[32], previous_radius;
    struct object_datum *object;
    real_point3d center;
    real radius;
    float frames[32];
    long root;
    int mode = setting(), i, mask = 0, remaining = 128;
    if (!mode || !cached_bsp || !parameters || global_window_parameters.rasterizer_target != 0 ||
        !rasterizer_debug_options.draw_environment_shadows ||
        first_person) goto unavailable;
    root = parameters->unique_identifier;
    for (i = 0; i < 128; i++)
    {
        object = object_try_and_get(root);
        if (!object) goto unavailable;
        if (object->object.parent_object_index == NONE) break;
        root = object->object.parent_object_index;
    }
    if (i == 128 || TEST_FLAG(object->object.flags, _object_shadowless_bit)) goto unavailable;
    object_get_bounding_sphere(root, &center, &radius);
    render_interpolation_object_point(root, &center);
    if (!(radius > 0.001f && radius < REAL_MAX)) goto unavailable;
    if (!environmental_family_bounds(root, &center, &radius, &remaining, 0)) goto unavailable;
    remaining = 128;
    memset(frames, 0, sizeof(frames));
    for (i = 0; i < MIN(parameters->lighting.distant_light_count, 2); i++)
    {
        struct render_distant_light const *light = &parameters->lighting.distant_lights[i];
        real_vector3d direction = light->direction, right;
        real_matrix4x3 frame;
        if (light->color.red <= 0 && light->color.green <= 0 && light->color.blue <= 0) continue;
        if (!(normalize3d(&direction) > 0.001f)) continue;
        perpendicular3d(&direction, &right);
        normalize3d(&right);
        matrix4x3_from_point_and_vectors(&frame, &center, &right, &direction);
        matrix_to_gl(&frame, frames + i * 16);
        mask |= 1 << i;
    }
    if (!mask) goto unavailable;
    if (previous == root && previous_radius == radius &&
        !memcmp(previous_frames, frames, sizeof(frames)) && modern_environment_model_reuse()) return;
    previous = root;
    previous_radius = radius;
    memcpy(previous_frames, frames, sizeof(frames));
    if (modern_environment_model_begin(frames, radius, mode == 2))
        modern_environment_model_end(environmental_casters(root, &remaining), mask);
    return;
unavailable:
    previous = NONE;
    modern_environment_model_clear();
}

void modern_shadows_environment_model(const struct rasterizer_model_begin_parameters *parameters, int first_person)
{
    environment_model_family(parameters,first_person);
    if (cached_bsp && prepared_sky != NONE && parameters && !first_person)
    {
        struct object_datum *object = object_try_and_get(parameters->unique_identifier);
        modern_environment_sun_model(object && sun_cluster_sky(object->object.location.cluster_index) == prepared_sky);
    }
}

int modern_shadows_environment(long object, const real_matrix4x3 *frame, const float color[3], float radius)
{
    float basis[16], camera[16];
    int remaining = 128, complete, handoff, mode = setting();
    if (!mode || !cached_bsp || global_window_parameters.rasterizer_target != 0 /* primary, as in the stock shadow pass */ ||
        !rasterizer_debug_options.draw_environment_shadows || !(radius > 0.001f && radius < REAL_MAX)) return 0;
    matrix_to_gl(frame, basis);
    handoff = sun_owns(object);
    if (handoff && modern_environment_sun_interior(basis,radius)) return 1;
    matrix_to_gl(&global_window_parameters.frustum.view_to_world, camera);
    if (!modern_environment_begin(basis, radius, mode == 2)) return 0;
    complete = environmental_casters(object, &remaining);
    return modern_environment_end(complete, camera, (float const *)global_window_parameters.frustum.projection_matrix, color, handoff);
}

void modern_shadows_prepare(int primary)
{
	struct structure_bsp const *bsp;
	int mode = setting(), complete, slot;
	long owners[2];
	active = world_stage = 0;
	prepared_sky = NONE;
	sun_count = flash_world = 0;
	flashlight_index = NONE;
	modern_environment_window();
	if (!mode) { if (cached_bsp) modern_shadows_reset(); return; }
	if (!primary) return;
	bsp = global_structure_bsp_get();
	if (bsp != cached_bsp)
	{
		modern_shadow_change_bsp();
		forget_shadow_geometry();
		cached_bsp = bsp;
	}
	if (!bsp || receiver_failed) return;
	check_static_breakables(bsp);
	extern boolean render_shadows;
	if (render_shadows) sun_prepare();
	prepare_spots(mode);
	select_lights();
	for (slot = 0; slot < MODERN_SHADOW_LIGHTS; slot++)
	{
		long index = selections[slot].rasterizer;
		struct rasterizer_light_submit_parameters const *light;
		float radius;
		struct plasma_shadow_source grenade;
		long excluded_caster;
		if (index == NONE) continue;
		selections[slot].unshadowed_owner = plasma_lighting_shadow_owner(selections[slot].identity, &grenade);
		excluded_caster = muzzle_lighting_shadow_caster(selections[slot].identity, grenade.emitter_object, &selections[slot].collision_owner);
		light = &rasterizer_lights.lights[index];
		/* Diffuse and specular share the map, including the larger extent. */
		radius = light->radius * MAX(1.f, plasma_occlusion_specular_scale(index));
		if (!modern_shadow_begin(slot, (float const *)&light->position, radius, mode == 2, grenade.grounded)) continue;
		/* Caster exclusions leave both the shooter and the weapon as receivers. */
		if (grenade.stuck_host != NONE) excluded_caster = grenade.stuck_host;
		complete = viewmodel_casters(owners, excluded_caster, selections[slot].unshadowed_owner) && world_casters(bsp, &light->position, radius) &&
			object_casters(&light->position, radius, owners, selections[slot].unshadowed_owner, excluded_caster, grenade.casing);
		if (modern_shadow_end() && complete)
		{
			view_unit = owners[1];
			active |= 1 << slot;
		}
	}
}

#else
int modern_shadows_enabled(void) { return 0; }
int modern_shadows_hard_enabled(void) { return 0; }
void modern_shadows_sun_project(void) {}
void modern_spots_project(void) {}
void modern_flashlight_mount(long wearer, struct rasterizer_light_submit_parameters *light) {}
short modern_flashlight_include(short view, long *lights, short count, short capacity) { return count; }
int modern_spot_is_flashlight(long index) { return 0; }
void modern_spot_world_light(long index) {}
int modern_shadows_sun_casters(void) { return 0; }
void modern_shadows_environment_model(const struct rasterizer_model_begin_parameters *p, int f) {}
void modern_shadows_environment_receivers(int primary) {}
int modern_shadows_environment(long o, const struct real_matrix4x3 *f, const float c[3], float r) { return 0; }
void modern_shadows_receiver(int first_person, long object) {}
int modern_shadows_view_receiver(void) { return 0; }
int modern_shadows_map(int slot) { return -1; }
void modern_shadows_map_loaded(int gameplay) {}
int modern_shadows_prepare_loaded_map(void) { return 0; }
void modern_shadows_prepare(int primary) {}
void modern_shadows_reset(void) {}
void modern_shadows_window(void) {}
void modern_shadows_submitted(long identity, long index) {}
void modern_shadows_model_slot(long index, int slot) {}
void modern_shadows_world_begin(long index, int stage) {}
void modern_shadows_world_end(void) {}
unsigned int modern_shadows_model_mask(void) { return 0; }
int modern_shadows_selected(long index) { return 0; }
long modern_shadows_collision_owner(long index) { return -1; }
void modern_shadows_cancel(void) {}
int modern_shadows_world_stage(void) { return 0; }
float modern_shadows_world_extent(void) { return 0; }
#endif
