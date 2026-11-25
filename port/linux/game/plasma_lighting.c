/* Optional rendering for authored projectile lights. Reuse the rendered
   attachment pose without editing tags or advancing projectile simulation.
   rocket_lighting.c owns additional flight lights where rockets have none. */
#include "cseries.h"
#include "objects/objects.h"
#include "items/projectiles.h"
#include "tag_files/tag_files.h"
#include "plasma_lighting.h"
#include "authored_emitter_lighting.h"
#include "plasma_model_shader.h"
#include "plasma_occlusion.h"
#include "modern_shadows.h"
#include "rocket_lighting.h"
#include "covenant_lighting.h"
#include "covenant_emitter_lighting.h"
#include "muzzle_lighting.h"
#include "shield_glow.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include <string.h>

#include "lighting_config.h"

enum { PLASMA_LIGHTING_MAXIMUM = 128 };
static struct
{
	long light_index;
	long object_index;
	long rasterizer_index;
	int was_dynamic, category, owner_unshadowed, exposed_grenade;
	float radius_scale, saved_radius, forward_offset;
} plasma_lighting_attachments[PLASMA_LIGHTING_MAXIMUM];
static int plasma_lighting_count;
static unsigned int	plasma_model_mask;

void plasma_model_lighting_reset(void)
{
	plasma_model_mask = 0;
	plasma_occlusion_model_slot(NONE, 0);
	plasma_occlusion_model_slot(NONE, 1);
	modern_shadows_model_slot(NONE, 0);
	modern_shadows_model_slot(NONE, 1);
}

void plasma_model_lighting_slot(long light_index, short slot)
{
	if (slot < 0 || slot >= 2)
		return;
	plasma_model_mask &= ~(1U << slot);
	plasma_occlusion_model_slot(light_index, slot);
	modern_shadows_model_slot(light_index, slot);
	if (plasma_lighting_selected(light_index))
		plasma_model_mask |= 1U << slot;
}

unsigned int plasma_model_lighting_mask(void)
{
	return (lighting_config_boolean("display.plasma_lighting") || lighting_config_boolean("display.muzzle_lighting") ||
		covenant_emitter_lighting_enabled() || authored_emitter_lighting_enabled() || shield_glow_enabled()) ? plasma_model_mask : 0;
}

void plasma_lighting_prepare(long light_index, long object_index, long definition_index)
{
	struct object_datum *object;
	const char *name;
	int index, category, owner_unshadowed;
	float covenant_scale, authored_scale = 1.0f, forward_offset = 0.f;

	if ((!lighting_config_boolean("display.plasma_lighting") && !authored_emitter_lighting_enabled()) || object_index == NONE)
		return;
	if (covenant_lighting_owns(light_index, object_index))
		return; /* Owned glows have their own radius and frame cleanup. */
	object = object_try_and_get(object_index);
	if (!object)
		return;
	category = authored_emitter_lighting_profile(object_index, definition_index, &authored_scale, &forward_offset, &owner_unshadowed);
	if (!category && (!lighting_config_boolean("display.plasma_lighting") || object->object.type != _object_type_projectile))
		return;
	name = tag_get_name(object->definition_index);
	covenant_scale = covenant_lighting_scale(name);
	/* Exact tag policies keep unrelated custom projectiles stock. */
	/* The pistol overcharge uses a tag in the rifle directory. */
	if (!category && (!name || (!covenant_scale && strcmp(name, "weapons\\plasma rifle\\bolt") &&
		strcmp(name, "weapons\\plasma pistol\\bolt") &&
		strcmp(name, "weapons\\plasma rifle\\charged bolt") &&
		strcmp(name, "weapons\\rocket launcher\\rocket"))))
		return;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index &&
			plasma_lighting_attachments[index].object_index == object_index)
			return; /* shared by split-screen views */
	if (plasma_lighting_count == PLASMA_LIGHTING_MAXIMUM)
		return; /* excess lights retain stock behavior */
	if (!light_port_attachment_radius(light_index, object_index,
		&plasma_lighting_attachments[plasma_lighting_count].saved_radius, FALSE))
		return;
	plasma_lighting_attachments[plasma_lighting_count].light_index = light_index;
	plasma_lighting_attachments[plasma_lighting_count].was_dynamic = light_port_dynamic_set(light_index, object_index, TRUE);
	plasma_lighting_attachments[plasma_lighting_count].rasterizer_index = NONE;
	plasma_lighting_attachments[plasma_lighting_count].object_index = object_index;
	/* Wider soft falloff increases useful illumination without clipping colour.
	   Pistol shots start with smaller authored radii than rifle bolts. */
	plasma_lighting_attachments[plasma_lighting_count].radius_scale =
		category ? authored_scale : covenant_scale ? covenant_scale :
		!strcmp(name, "weapons\\plasma pistol\\bolt") ? 2.25f :
		!strcmp(name, "weapons\\plasma rifle\\charged bolt") ? 2.75f : 1.75f;
	plasma_lighting_attachments[plasma_lighting_count].category = category;
	plasma_lighting_attachments[plasma_lighting_count].owner_unshadowed = owner_unshadowed;
	plasma_lighting_attachments[plasma_lighting_count].forward_offset = forward_offset;
	plasma_lighting_attachments[plasma_lighting_count].exposed_grenade =
		category == AUTHORED_EMITTER_PLASMA_GRENADE && object->object.parent_object_index != NONE &&
		modern_shadows_hard_enabled();
	plasma_lighting_count++;
	if (!light_port_refresh_attachment(light_index, object_index))
	{
		/* Disconnected engine attachments stay out of render selection. */
		plasma_lighting_count--;
		light_port_dynamic_set(light_index, object_index,
			plasma_lighting_attachments[plasma_lighting_count].was_dynamic);
	}
}

/* Apply at the marker-to-world boundary, before cluster reconnection. CPU
   selection, shadow maps and illumination must share one render-time origin.
   Only registered attachments can move; cleanup clears the offset before
   reconnecting their stock simulation pose. */
void plasma_lighting_attachment_position(long light_index, long object_index,
	real_point3d *position, real_vector3d const *forward, float scale)
{
	int index;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index &&
			plasma_lighting_attachments[index].object_index == object_index)
		{
			float offset = plasma_lighting_attachments[index].forward_offset * scale;
			if (plasma_lighting_attachments[index].category == AUTHORED_EMITTER_BANSHEE_WING)
				authored_emitter_wing_position(light_index, object_index, position);
			if (plasma_lighting_attachments[index].exposed_grenade)
			{
				/* Model-space point just beyond the luminous inset and casing.
				   The render pose moves the light with the attached projectile. */
				static const real_point3d exposed = { -0.00602118f, -0.00510639f, 0.02301373f };
				matrix4x3_transform_point(object_get_node_matrix(object_index, 0), &exposed, position);
				return;
			}
			if (offset && authored_emitter_lighting_category_enabled(plasma_lighting_attachments[index].category))
			{
				position->x += offset * forward->i;
				position->y += offset * forward->j;
				position->z += offset * forward->k;
			}
			return;
		}
}

float plasma_lighting_radius(long light_index, long object_index, float radius)
{
	int index;
	if (!lighting_config_boolean("display.plasma_lighting") && !authored_emitter_lighting_enabled())
		return radius;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index &&
			plasma_lighting_attachments[index].object_index == object_index &&
			authored_emitter_lighting_category_enabled(plasma_lighting_attachments[index].category))
			return radius * plasma_lighting_attachments[index].radius_scale;
	return radius;
}

/* Full datum identities keep this exception local to one live light/owner
   pair. The shadow collector resolves it once per selected map, not per draw. */
long plasma_lighting_shadow_owner(long light, struct plasma_shadow_source *grenade)
{
	int i;
	long owner = covenant_emitter_lighting_shadow_owner(light, &grenade->emitter_object);
	grenade->grounded = FALSE;
	grenade->casing = grenade->stuck_host = NONE;
	if (grenade->emitter_object != NONE || light == NONE) return owner;
	for (i = 0; i < plasma_lighting_count; i++)
	{
		struct object_datum *object;
		if (plasma_lighting_attachments[i].light_index != light) continue;
		if (!authored_emitter_lighting_category_enabled(plasma_lighting_attachments[i].category)) break;
		object = object_try_and_get(plasma_lighting_attachments[i].object_index);
		if (!object) break;
		grenade->emitter_object = plasma_lighting_attachments[i].object_index;
		if (plasma_lighting_attachments[i].category == AUTHORED_EMITTER_PLASMA_GRENADE)
		{
			grenade->casing = plasma_lighting_attachments[i].object_index;
			grenade->grounded = object->object.parent_object_index == NONE && TEST_FLAG(object->object.flags, _object_at_rest_bit);
			/* A grenade also has a parent while held before release. */
			if (object->object.type == _object_type_projectile &&
				projectile_is_attached((struct projectile_datum const *)object))
				grenade->stuck_host = object->object.parent_object_index;
		}
		if (plasma_lighting_attachments[i].owner_unshadowed) return plasma_lighting_attachments[i].object_index;
		break;
	}
	return shield_glow_shadow_owner(light);
}

void plasma_lighting_frame_end(void)
{
	int index;

	shield_glow_frame_end();
	muzzle_lighting_frame_end();
	covenant_emitter_lighting_frame_end();
	covenant_lighting_frame_end();
	rocket_lighting_frame_end();
	plasma_falloff_end(); /* Also close a pass if drawing ended early. */
	/* Called after render interpolation ends, even if the option was disabled.
	   Restore positions AND cluster membership before the next simulation tick. */
	for (index = 0; index < plasma_lighting_count; index++)
	{
		plasma_lighting_attachments[index].category = 0;
		plasma_lighting_attachments[index].radius_scale = 1.0f;
		plasma_lighting_attachments[index].forward_offset = 0.f;
		plasma_lighting_attachments[index].exposed_grenade = FALSE;
		light_port_attachment_radius(plasma_lighting_attachments[index].light_index,
			plasma_lighting_attachments[index].object_index,
			&plasma_lighting_attachments[index].saved_radius, TRUE);
		light_port_dynamic_set(plasma_lighting_attachments[index].light_index,
			plasma_lighting_attachments[index].object_index, plasma_lighting_attachments[index].was_dynamic);
		light_port_refresh_attachment(plasma_lighting_attachments[index].light_index,
			plasma_lighting_attachments[index].object_index);
	}
	plasma_lighting_reset();
	if (!lighting_config_boolean("display.plasma_occlusion") && !lighting_config_boolean("display.muzzle_lighting") &&
		!covenant_emitter_lighting_enabled() && !authored_emitter_lighting_enabled() && !shield_glow_enabled())
		plasma_occlusion_reset();
	if (!lighting_config_boolean("display.plasma_lighting") && !lighting_config_boolean("display.muzzle_lighting") &&
		!covenant_emitter_lighting_enabled() && !authored_emitter_lighting_enabled() && !shield_glow_enabled() &&
        !modern_shadows_enabled())
        /* Environmental grounding remains active without local feature lights.
           Map/shutdown disposal still releases the complete shadow owner. */
		plasma_falloff_dispose();
}

void plasma_lighting_reset(void)
{
	/* No tag pointers or object identities survive unloading or light recovery. */
	plasma_lighting_count = 0;
	plasma_model_lighting_reset();
}

void plasma_lighting_window_begin(void)
{
	int index;
	modern_shadows_window();
	plasma_occlusion_window();
	shield_glow_window_begin();
	muzzle_lighting_window_begin();
	covenant_emitter_lighting_window_begin();
	covenant_lighting_window_begin();
	rocket_lighting_window_begin();
	plasma_model_lighting_reset();
	for (index = 0; index < plasma_lighting_count; index++)
		plasma_lighting_attachments[index].rasterizer_index = NONE;
}

void plasma_lighting_submitted(long light_index, long rasterizer_index)
{
	int index;
	modern_shadows_submitted(light_index, rasterizer_index);
	shield_glow_submitted(light_index, rasterizer_index);
	muzzle_lighting_submitted(light_index, rasterizer_index);
	covenant_emitter_lighting_submitted(light_index, rasterizer_index);
	covenant_lighting_submitted(light_index, rasterizer_index);
	rocket_lighting_submitted(light_index, rasterizer_index);
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index)
		{
			plasma_lighting_attachments[index].rasterizer_index = rasterizer_index;
			plasma_occlusion_submit(light_index, rasterizer_index);
		}
}

int plasma_lighting_selected(long rasterizer_index)
{
	int index;
	/* Shared feature-light eligibility despite the historical module name:
	   emitters, held glows and muzzle flashes participate without projectiles. */
	if (shield_glow_selected(rasterizer_index) || muzzle_lighting_selected(rasterizer_index) || covenant_emitter_lighting_selected(rasterizer_index))
		return TRUE;
	if (rasterizer_index == NONE || (!lighting_config_boolean("display.plasma_lighting") && !authored_emitter_lighting_enabled()))
		return FALSE;
	if (covenant_lighting_selected(rasterizer_index) || rocket_lighting_selected(rasterizer_index))
		return TRUE;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].rasterizer_index == rasterizer_index &&
			authored_emitter_lighting_category_enabled(plasma_lighting_attachments[index].category))
			return TRUE;
	return FALSE;
}

/* Headlights include their own vehicle in the collision field, even when
   the general emitter occlusion option is off. Keep the authored beam cone. */
int plasma_lighting_vehicle_occlusion(long light_index, long *ignore)
{
	int index;
	if (light_index == NONE || !lighting_config_boolean("display.vehicle_lighting"))
		return FALSE;
	for (index = 0; index < plasma_lighting_count; index++)
		if (plasma_lighting_attachments[index].light_index == light_index &&
			(plasma_lighting_attachments[index].category == 2 ||
			 plasma_lighting_attachments[index].category == AUTHORED_EMITTER_BANSHEE_WING))
		{
			if (ignore && plasma_lighting_attachments[index].category == AUTHORED_EMITTER_BANSHEE_WING)
				*ignore = plasma_lighting_attachments[index].object_index;
			return TRUE;
		}
	return FALSE;
}
