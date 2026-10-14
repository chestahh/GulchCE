/* A filtered local visibility field avoids rejecting a whole model at a door.
   Collision queries see only the structure, scenery and machines. The field
   is approximate within one cell of an edge; this is not a shadow renderer. */
#include "plasma_occlusion.h"
#include "cseries.h"
#include "physics/collisions.h"
#include "plasma_lighting.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include "rasterizer/rasterizer_swizzle.h"
#include <math.h>
#include <string.h>
#include <xtl.h>

int config_boolean(const char *name);
enum
{
	OCC_SIZE = 16,
	OCC_COUNT = 4096,
	OCC_LIGHTS = 4,
	OCC_SUBMITTED = 128
};
static struct occlusion_field
{
	long identity;
	unsigned long used, updated, window;
	real center[3], extent, radius, specular;
	real visibility[OCC_COUNT];
	IDirect3DVolumeTexture8 *texture[3];
	int valid;
} fields[OCC_LIGHTS];
static struct
{
	long identity, rasterizer;
} submitted[OCC_SUBMITTED];
static int submitted_count;
static int updates_this_window;
static unsigned long current_window;
static long model_slots[2] = {NONE, NONE};
static int enabled(void)
{
	return config_boolean("display.plasma_lighting") && config_boolean("display.plasma_occlusion");
}
static real sample_field(const real *values, real x, real y, real z)
{
	int a[3], b[3], i, j, k;
	real t[3], coordinate[3], result = 0;
	coordinate[0] = x;
	coordinate[1] = y;
	coordinate[2] = z;
	for (i = 0; i < 3; i++)
	{
		real p = MAX(0.0f, MIN((real)(OCC_SIZE - 1), coordinate[i] * OCC_SIZE - 0.5f));
		a[i] = (int)p;
		b[i] = MIN(a[i] + 1, OCC_SIZE - 1);
		t[i] = p - a[i];
	}
	for (k = 0; k < 2; k++)
		for (j = 0; j < 2; j++)
			for (i = 0; i < 2; i++)
				result += values[((k ? b[2] : a[2]) * OCC_SIZE + (j ? b[1] : a[1])) * OCC_SIZE +
								 (i ? b[0] : a[0])] *
						  (i ? t[0] : 1 - t[0]) * (j ? t[1] : 1 - t[1]) * (k ? t[2] : 1 - t[2]);
	return result;
}
static real trace_visibility(real_point3d const *from, real_vector3d const *vector)
{
	struct collision_result hit;
	real length = sqrtf(vector->i * vector->i + vector->j * vector->j + vector->k * vector->k);
	unsigned long flags =
		FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit) | FLAG(_collision_test_objects_bit) |
		FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit);
	if (length < 0.0001f || !collision_test_vector(flags, from, vector, NONE, &hit))
		return 1;
	/* Leave a narrow receiver bias, without admitting surfaces far past a wall. */
	return MAX(0.0f, MIN(1.0f, 1.0f - (1.0f - hit.t) * length / 0.025f));
}
static int upload(IDirect3DVolumeTexture8 **texture, unsigned long const *pixels)
{
	D3DLOCKED_BOX box;
	if (!*texture && D3DDevice_CreateVolumeTexture(OCC_SIZE, OCC_SIZE, OCC_SIZE, 1, 0, D3DFMT_A8R8G8B8,
												   D3DPOOL_MANAGED, texture) < 0)
		return 0;
	if (IDirect3DVolumeTexture8_LockBox(*texture, 0, &box, NULL, 0) < 0 || !box.pBits)
		return 0;
	rasterizer_xbox_bitmap_swizzle3d_long(box.pBits, (void *)pixels, OCC_SIZE, OCC_SIZE, OCC_SIZE);
	return IDirect3DVolumeTexture8_UnlockBox(*texture, 0) >= 0;
}
static struct occlusion_field *field_for(long rasterizer_index)
{
	struct occlusion_field *field = NULL;
	struct rasterizer_light_submit_parameters *light;
	unsigned long now = GetTickCount(), pixels[OCC_COUNT];
	long identity = NONE;
	int i, x, y, z, pass;
	real elapsed, extent, specular, clear_alpha, block_alpha;
	if (!enabled() || rasterizer_index < 0 || rasterizer_index >= rasterizer_lights.light_count)
		return NULL;
	for (i = 0; i < submitted_count; i++)
		if (submitted[i].rasterizer == rasterizer_index)
			identity = submitted[i].identity;
	if (identity == NONE)
		return NULL;
	for (i = 0; i < OCC_LIGHTS; i++)
		if (fields[i].valid && fields[i].identity == identity)
			field = &fields[i];
	if (field && field->window == current_window)
		return field;
	if (updates_this_window >= OCC_LIGHTS)
		return NULL;
	if (!field)
	{
		for (i = 0; i < OCC_LIGHTS; i++)
			if (!fields[i].valid || now - fields[i].used > 100)
			{
				field = &fields[i];
				break;
			}
		if (!field)
			return NULL; /* stable admission: active lights are never evicted */
		field->valid = 0;
		field->identity = identity;
		for (i = 0; i < OCC_COUNT; i++)
			field->visibility[i] = 1;
	}
	field->used = now;
	light = &rasterizer_lights.lights[rasterizer_index];
	specular = plasma_occlusion_specular_scale(rasterizer_index);
	extent = light->radius * MAX(1.0f, specular);
	if (!(extent > 0.001f && extent < 1000.0f))
		return NULL;
	if (field->valid && field->window == current_window)
		return field;
	updates_this_window++;
	elapsed = field->valid ? MIN((real)(now - field->updated) * 0.001f, 0.1f) : 0.1f;
	clear_alpha = 1.0f - powf(0.5f, elapsed / 0.025f);
	block_alpha = 1.0f - powf(0.5f, elapsed / 0.008f);
	for (z = 0; z < OCC_SIZE; z++)
		for (y = 0; y < OCC_SIZE; y++)
			for (x = 0; x < OCC_SIZE; x++)
			{
				real_vector3d ray;
				real target, alpha;
				i = (z * OCC_SIZE + y) * OCC_SIZE + x;
				ray.i = (2.0f * (x + 0.5f) / OCC_SIZE - 1) * extent;
				ray.j = (2.0f * (y + 0.5f) / OCC_SIZE - 1) * extent;
				ray.k = (2.0f * (z + 0.5f) / OCC_SIZE - 1) * extent;
				target = trace_visibility(&light->position, &ray);
				/* Time-based filtering, faster when a blocker appears than when it clears. */
				alpha = target < field->visibility[i] ? block_alpha : clear_alpha;
				field->visibility[i] += (target - field->visibility[i]) * alpha;
			}
	field->center[0] = light->position.x;
	field->center[1] = light->position.y;
	field->center[2] = light->position.z;
	field->radius = light->radius;
	field->specular = specular;
	field->extent = extent;
	for (pass = 0; pass < 3; pass++)
	{
		for (z = 0; z < OCC_SIZE; z++)
			for (y = 0; y < OCC_SIZE; y++)
				for (x = 0; x < OCC_SIZE; x++)
				{
					real dx = 2.0f * (x + 0.5f) / OCC_SIZE - 1, dy = 2.0f * (y + 0.5f) / OCC_SIZE - 1,
						 dz = 2.0f * (z + 0.5f) / OCC_SIZE - 1;
					real scale = (pass == 2 ? specular : 1.0f) * light->radius / extent;
					real remaining = MAX(0.0f, 1 - dx * dx - dy * dy - dz * dz);
					real value = pass ? sample_field(field->visibility, 0.5f + dx * scale * 0.5f,
													 0.5f + dy * scale * 0.5f, 0.5f + dz * scale * 0.5f) *
											remaining * remaining
									  : field->visibility[(z * OCC_SIZE + y) * OCC_SIZE + x];
					pixels[(z * OCC_SIZE + y) * OCC_SIZE + x] =
						(unsigned long)(255 * value + 0.5f) * 0x01010101UL;
				}
		if (!upload(&field->texture[pass], pixels))
		{
			field->valid = 0;
			return NULL;
		}
	}
	field->updated = now;
	field->window = current_window;
	field->valid = 1;
	return field;
}
void plasma_occlusion_reset(void)
{
	int i, j;
	for (i = 0; i < OCC_LIGHTS; i++)
		for (j = 0; j < 3; j++)
			if (fields[i].texture[j])
				D3DResource_Release((D3DResource *)fields[i].texture[j]);
	memset(fields, 0, sizeof(fields));
	plasma_occlusion_window();
}
void plasma_occlusion_window(void)
{
	current_window++;
	submitted_count = 0;
	updates_this_window = 0;
	model_slots[0] = model_slots[1] = NONE;
}
void plasma_occlusion_submit(long light, long rasterizer_index)
{
	if (enabled() && submitted_count < OCC_SUBMITTED)
	{
		submitted[submitted_count].identity = light;
		submitted[submitted_count++].rasterizer = rasterizer_index;
	}
}
void plasma_occlusion_model_slot(long rasterizer_index, short slot)
{
	if (slot >= 0 && slot < 2)
		model_slots[slot] = rasterizer_index;
}
void *plasma_occlusion_texture(long rasterizer_index, int specular)
{
	struct occlusion_field *field = field_for(rasterizer_index);
	return field ? field->texture[specular ? 2 : 1] : NULL;
}
void *plasma_occlusion_model_texture(int slot, float transform[4])
{
	struct occlusion_field *field;
	if (slot < 0 || slot >= 2)
		return NULL;
	field = field_for(model_slots[slot]);
	if (!field)
		return NULL;
	transform[0] = field->center[0];
	transform[1] = field->center[1];
	transform[2] = field->center[2];
	transform[3] = 0.5f / field->extent;
	return field->texture[0];
}
