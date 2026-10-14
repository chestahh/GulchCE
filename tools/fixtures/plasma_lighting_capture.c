/* Test-build-only fixture. The visual runner installs and removes this file.
			Creates real plasma rifle projectiles; never linked into the normal game. */
#include "cseries.h"
#include <xtl.h>
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "items/projectiles.h"
#include "items/projectile_definitions.h"
#include "camera/observer.h"
#include "tag_files/tag_groups.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include "plasma_lighting.h"
#include <stdlib.h>
#include <string.h>

void platform_log(const char *format, ...);
int config_boolean(const char *name);
real render_interpolation_fraction(void);
static long capture_ticks, capture_projectile = NONE;
static struct observer_result capture_camera;
static int capture_model;
static real capture_distance, capture_height, capture_receiver_distance;
static real capture_option(const char *name, real fallback)
{
	const char *value = getenv(name);
	return value ? (real)atof(value) : fallback;
}
static unsigned long capture_model_draws, capture_specular_draws;
static int capture_specular_selected;
void plasma_capture_model_draw(unsigned int mask)
{
	if (mask) capture_model_draws++;
}
void plasma_capture_specular_begin(long index)
{
	capture_specular_selected = plasma_lighting_selected(index);
}
void plasma_capture_specular_draw(void)
{
	if (capture_specular_selected) capture_specular_draws++;
}

void plasma_capture_tick(void)
{
	struct object_placement_data placement;
	real_vector3d right;
	long tag, i;
	capture_ticks++;
	if (capture_ticks == 60)
	{
		capture_camera = *observer_get_camera(0);
		capture_model = getenv("HALO_PLASMA_CAPTURE_MODEL") != NULL;
		capture_distance = capture_option("HALO_PLASMA_CAPTURE_DISTANCE", capture_model ? 2.2f : 3.0f);
		capture_height = capture_option("HALO_PLASMA_CAPTURE_HEIGHT", capture_model ? -0.2f : -0.6f);
		capture_receiver_distance = capture_option("HALO_PLASMA_CAPTURE_RECEIVER_DISTANCE", 2.8f);
		platform_log("PLASMA camera %.4f %.4f %.4f forward %.4f %.4f %.4f",
			capture_camera.position.x, capture_camera.position.y, capture_camera.position.z,
			capture_camera.forward.i, capture_camera.forward.j, capture_camera.forward.k);
	}
	if (capture_ticks == 65 && capture_model)
	{
		tag = tag_loaded('bipd', "characters\\cyborg_mp\\cyborg_mp");
		if (tag == NONE) tag = tag_loaded('bipd', "characters\\cyborg\\cyborg");
		if (tag != NONE)
		{
			object_placement_data_new(&placement, tag, NONE);
			placement.position.x = capture_camera.position.x + capture_receiver_distance*capture_camera.forward.i;
			placement.position.y = capture_camera.position.y + capture_receiver_distance*capture_camera.forward.j;
			placement.position.z = capture_camera.position.z - 0.6f;
			placement.forward = capture_camera.forward;
			placement.forward.i = -placement.forward.i; placement.forward.j = -placement.forward.j;
			observer_up_from_forward(&placement.forward, &placement.up);
			platform_log("PLASMA receiver model %ld tag %s", object_new(&placement), tag_get_name(tag));
		}
		else platform_log("PLASMA receiver missing");
	}
	if (capture_ticks < 90 || capture_ticks > 210 || capture_ticks % 60 != 30)
		return;
	tag = tag_loaded(PROJECTILE_DEFINITION_TAG, "weapons\\plasma rifle\\bolt");
	if (tag == NONE)
	{
		platform_log("PLASMA missing projectile tag");
		return;
	}
	cross_product3d(&capture_camera.forward, &capture_camera.up, &right);
	object_placement_data_new(&placement, tag, NONE);
	placement.position.x = capture_camera.position.x + capture_distance*capture_camera.forward.i - 1.2f*right.i + capture_height*capture_camera.up.i;
	placement.position.y = capture_camera.position.y + capture_distance*capture_camera.forward.j - 1.2f*right.j + capture_height*capture_camera.up.j;
	placement.position.z = capture_camera.position.z + capture_distance*capture_camera.forward.k - 1.2f*right.k + capture_height*capture_camera.up.k;
	placement.forward = right;
	placement.forward.k -= 0.12f;
	normalize3d(&placement.forward);
	observer_up_from_forward(&placement.forward, &placement.up);
	capture_projectile = object_new(&placement);
	platform_log("PLASMA shot tick %ld object %ld", capture_ticks, capture_projectile);
	if (capture_projectile != NONE)
	{
		struct object_datum *object = object_get(capture_projectile);
		struct object_definition *definition = object_definition_get(tag);
		struct object_attachment_definition *attachment = TAG_BLOCK_GET_ELEMENT(&definition->object.attachments, 0, struct object_attachment_definition);
		long *light = tag_get('ligh', attachment->type.index);
		platform_log("PLASMA tag %s type %d", tag_get_name(tag), object->object.type);
		platform_log("PLASMA enabled %d light flags %lx radius %.4f", config_boolean("display.plasma_lighting"), light[0], ((real *)light)[1]);
		for (i=0; i<MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT; i++)
			platform_log("PLASMA attachment %ld type %d index %ld", i, object->object.attachment_types[i], object->object.attachment_indices[i]);
	}
}

void plasma_capture_frame(void)
{
	long index, selected = 0;
	real_matrix4x3 *matrix;
	if (capture_ticks < 85 || capture_ticks > 240)
		return;
	platform_log("PLASMA draws model %lu specular %lu", capture_model_draws, capture_specular_draws);
	capture_model_draws = capture_specular_draws = 0;
	for (index = 0; index < rasterizer_lights.light_count; index++)
		selected += plasma_lighting_selected(index);
	if (capture_projectile != NONE && object_try_and_get(capture_projectile))
	{
		matrix = object_get_node_matrix(capture_projectile, 0);
		platform_log("PLASMA frame %lu tick %ld fraction %.5f position %.5f %.5f %.5f selected %ld",
			GetTickCount(), capture_ticks, render_interpolation_fraction(),
			matrix->position.x, matrix->position.y, matrix->position.z, selected);
	}
	else
		platform_log("PLASMA frame %lu tick %ld no projectile selected %ld", GetTickCount(), capture_ticks, selected);
}

int plasma_capture_should_capture(void)
{
	return capture_ticks >= 85 && capture_ticks <= 240 && capture_ticks % 60 >= 28 && capture_ticks % 60 <= 58;
}
