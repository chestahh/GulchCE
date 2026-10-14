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

void platform_log(const char *format, ...);
int config_boolean(const char *name);
real render_interpolation_fraction(void);
static long capture_ticks, capture_projectile = NONE;
static struct observer_result capture_camera;

void plasma_capture_tick(void)
{
	struct object_placement_data placement;
	real_vector3d right;
	long tag, i;
	capture_ticks++;
	if (capture_ticks == 60)
	{
		capture_camera = *observer_get_camera(0);
		platform_log("PLASMA camera %.4f %.4f %.4f forward %.4f %.4f %.4f",
			capture_camera.position.x, capture_camera.position.y, capture_camera.position.z,
			capture_camera.forward.i, capture_camera.forward.j, capture_camera.forward.k);
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
	placement.position.x = capture_camera.position.x + 3.0f*capture_camera.forward.i - 1.2f*right.i - 0.6f*capture_camera.up.i;
	placement.position.y = capture_camera.position.y + 3.0f*capture_camera.forward.j - 1.2f*right.j - 0.6f*capture_camera.up.j;
	placement.position.z = capture_camera.position.z + 3.0f*capture_camera.forward.k - 1.2f*right.k - 0.6f*capture_camera.up.k;
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
