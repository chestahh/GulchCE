/* Optional rendering prototype for the plasma rifle's existing lights.
   Reuse the rendered attachment pose, including its marker offset. Never
   create a second light, change a tag, or advance projectile simulation. */
#include "cseries.h"
#include "objects/objects.h"
#include "tag_files/tag_files.h"
#include "plasma_lighting.h"
#include <string.h>

int config_boolean(const char *name);

enum { PLASMA_LIGHTING_MAXIMUM = 128 };
static struct
{
	long light_index;
	long object_index;
	long rasterizer_index;
	int was_dynamic;
} plasma_lighting_attachments[PLASMA_LIGHTING_MAXIMUM];
static int plasma_lighting_count;

void plasma_lighting_prepare(long light_index, long object_index)
{
	struct object_datum *object;
	const char *name;
	int index;

	if (!config_boolean("display.plasma_lighting") || object_index == NONE)
		return;
	object = object_try_and_get(object_index);
	if (!object || object->object.type != _object_type_projectile)
		return;
	name = tag_get_name(object->definition_index);
	/* Deliberately narrow: custom projectiles need an explicit policy later. */
	if (!name || strcmp(name, "weapons\\plasma rifle\\bolt"))
		return;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index &&
			plasma_lighting_attachments[index].object_index == object_index)
			return; /* shared by split-screen views */
	if (plasma_lighting_count == PLASMA_LIGHTING_MAXIMUM)
		return; /* excess lights retain stock behavior */
	if (!light_port_refresh_attachment(light_index, object_index))
		return;
	plasma_lighting_attachments[plasma_lighting_count].light_index = light_index;
	plasma_lighting_attachments[plasma_lighting_count].was_dynamic = light_port_dynamic_set(light_index, object_index, TRUE);
	plasma_lighting_attachments[plasma_lighting_count].rasterizer_index = NONE;
	plasma_lighting_attachments[plasma_lighting_count++].object_index = object_index;
}

void plasma_lighting_frame_end(void)
{
	int index;

	plasma_falloff_end(); /* Also close a pass if drawing ended early. */
	/* Called after render interpolation ends, even if the option was disabled.
	   Restore positions AND cluster membership before the next simulation tick. */
	for (index = 0; index < plasma_lighting_count; index++)
	{
		light_port_dynamic_set(plasma_lighting_attachments[index].light_index,
			plasma_lighting_attachments[index].object_index, plasma_lighting_attachments[index].was_dynamic);
		light_port_refresh_attachment(plasma_lighting_attachments[index].light_index,
			plasma_lighting_attachments[index].object_index);
	}
	plasma_lighting_reset();
	if (!config_boolean("display.plasma_lighting"))
		plasma_falloff_dispose();
}

void plasma_lighting_reset(void)
{
	/* No tag pointers or object identities survive unloading or light recovery. */
	plasma_lighting_count = 0;
}

void plasma_lighting_window_begin(void)
{
	int index;
	for (index = 0; index < plasma_lighting_count; index++)
		plasma_lighting_attachments[index].rasterizer_index = NONE;
}

void plasma_lighting_submitted(long light_index, long rasterizer_index)
{
	int index;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index)
			plasma_lighting_attachments[index].rasterizer_index = rasterizer_index;
}

int plasma_lighting_selected(long rasterizer_index)
{
	int index;
	if (rasterizer_index == NONE || !config_boolean("display.plasma_lighting"))
		return FALSE;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].rasterizer_index == rasterizer_index)
			return TRUE;
	return FALSE;
}
