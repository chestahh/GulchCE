/* Render-owned illumination from visible Covenant shields and fuel rods.
   No tags, shield state, weapon effects or simulation lights are changed. */
#include "cseries.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/object_lights.h"
#include "models/model_definitions.h"
#include "items/weapons.h"
#include "units/units.h"
#include "camera/observer.h"
#include "game/players.h"
#include "covenant_emitter_lighting.h"
#include "ammo_counter_lighting.h"
#include "plasma_occlusion.h"
#include <math.h>
#include <string.h>

int config_boolean(const char *name);
real render_interpolation_game_time_sec(long ticks);
long light_port_glow_definition_get(void);
boolean light_port_glow_set(long light, long owner, real_rgb_color const *color, real radius);
boolean light_port_glow_move(long light, real_point3d const *position);

enum { EMITTER_LIGHTS = 8 };
static struct emitter_light
{
	int active, connected, seen, kind;
	long object, light, rasterizer;
	real strength, last_time, radius;
} emitters[EMITTER_LIGHTS];
static const real_rgb_color emitter_black = {0, 0, 0};
struct emitter_sample
{
	int kind;
	real strength, radius;
	real_rgb_color color;
	real_point3d position;
};

int covenant_emitter_lighting_enabled(void)
{
	return config_boolean("display.jackal_shield_lighting") || config_boolean("display.fuel_rod_lighting") ||
		config_boolean("display.covenant_shield_lighting") || config_boolean("display.ammo_counter_lighting");
}

static int emitter_kind_enabled(int kind)
{
	return config_boolean(kind == 3 ? "display.ammo_counter_lighting" : kind == 2 ? "display.covenant_shield_lighting" :
		kind == 1 ? "display.fuel_rod_lighting" : "display.jackal_shield_lighting");
}

static int emitter_shield_visible(struct object_datum *object)
{
	struct object_definition *definition = object_definition_get(object->definition_index);
	struct model *model;
	int i;
	if (definition->object.model.index == NONE ||
		(object->object.maximum_body_vitality > 0.0f && object->object.body_vitality <= 0.0f) ||
		object->object.shield_vitality <= 0.0f ||
		TEST_FLAG(object->object.damage_flags, _object_shield_depleted_bit))
		return FALSE;
	model = model_definition_get(definition->object.model.index);
	for (i = 0; i < model->regions.count && i < MAXIMUM_REGIONS_PER_OBJECT; i++)
	{
		struct model_region *region = TAG_BLOCK_GET_ELEMENT(&model->regions, i, struct model_region);
		if (!strcmp(region->name, "shield"))
			/* Stock base00 is permutation zero; ~shield_off is one. The
			   shield-light marker itself also exists in the off permutation. */
			return object->object.region_permutations[i] == 0 &&
				!TEST_FLAG(object->object.regions_destroyed_flags, i);
	}
	return FALSE;
}

static int emitter_sample(long index, struct emitter_sample *sample)
{
	struct object_datum *object = object_try_and_get(index);
	struct object_marker marker;
	const char *name;
	const char *marker_name;
	real nearest = 25.0f;
	short player;
	if (!object || TEST_FLAG(object->object.flags, _object_invisible_bit) ||
		object->object.node_matrices.size < sizeof(real_matrix4x3))
		return FALSE;
	name = tag_get_name(object->definition_index);
	if (!name)
		return FALSE;
	if ((object->object.type == _object_type_biped &&
		(!strcmp(name, "characters\\jackal\\jackal") || !strcmp(name, "characters\\jackal\\jackal major"))) ||
		(object->object.type == _object_type_scenery && !strcmp(name, "scenery\\c_field_generator\\c_field_generator")))
	{
		sample->kind = object->object.type == _object_type_scenery ? 2 : 0;
		sample->radius = sample->kind == 2 ? 2.8f : 3.6f;
		marker_name = "shield light";
		/* The stock shield shader's colour source is change colour C. */
		sample->color = object->object.outgoing_change_colors[2];
		if (sample->kind == 0 && sample->color.blue > sample->color.red * 2.0f)
		{
			/* Blue needs more luminance and reach to illuminate nearby walls. */
			sample->color.red = MAX(sample->color.red, 0.30f);
			sample->color.green = MAX(sample->color.green, 0.42f);
		}
		if (sample->kind == 0 && sample->color.red > sample->color.blue * 2.0f)
		{
			/* Gold shares the blue shield's reach, with brighter warm spill. */
			sample->color.green = MAX(sample->color.green, 0.78f);
			sample->color.blue = MAX(sample->color.blue, 0.24f);
		}
		{
			/* Both stock shield combiners blend change colour C towards red
			   using A-out (stage 3). Use that evaluated animation value, not
			   vitality or our own hit timer, so damage/recovery stay in sync. */
			real flash = PIN(object->object.outgoing_function_values[0], 0.0f, 1.0f);
			sample->color.red += (1.0f - sample->color.red) * flash;
			sample->color.green *= 1.0f - flash;
			sample->color.blue *= 1.0f - flash;
		}
		sample->strength = emitter_shield_visible(object) ?
			0.7f + 0.15f * PIN(object->object.shield_vitality, 0.0f, 1.0f) +
			0.15f * PIN(object->object.recent_shield_damage, 0.0f, 1.0f) : 0.0f;
	}
	else if (object->object.type == _object_type_weapon && !strcmp(name, "weapons\\fuel rod gun\\fuel rod"))
	{
		/* The rod marker is on the back of this model, not at its muzzle.
		   Dropped rods remain luminous while their stock model still exists. */
		if (object->object.parent_object_index != NONE)
		{
			struct unit_datum *unit = unit_try_and_get(object->object.parent_object_index);
			if (!unit || TEST_FLAG(unit->object.flags, _object_invisible_bit) ||
				unit->unit.current_weapon_index < 0 || unit->unit.current_weapon_index >= MAXIMUM_WEAPONS_PER_UNIT ||
				unit_inventory_get_weapon(object->object.parent_object_index, unit->unit.current_weapon_index) != index)
				return FALSE;
		}
		sample->kind = 1;
		sample->radius = 1.6f;
		marker_name = "lens flare";
		sample->color.red = 0.15f;
		sample->color.green = 1.0f;
		sample->color.blue = 0.05f;
		sample->strength = 0.75f;
	}
	else if (object->object.type == _object_type_weapon && ammo_counter_lighting_position(index, &sample->position))
	{
		sample->kind = 3;
		sample->radius = 0.95f;
		sample->color.red = 0.24f;
		sample->color.green = 0.65f;
		sample->color.blue = 1.0f;
		sample->strength = 0.65f;
		marker_name = NULL;
	}
	else
		return FALSE;
	if (!emitter_kind_enabled(sample->kind))
		return FALSE;
	if (marker_name)
	{
		if (!object_get_marker_by_name(index, marker_name, &marker, 1))
			return FALSE;
		sample->position = marker.matrix.position;
	}
	for (player = local_player_get_next(NONE); player != NONE; player = local_player_get_next(player))
	{
		const struct observer_result *camera = observer_get_camera(player);
		if (camera)
			nearest = MIN(nearest, distance3d(&sample->position, &camera->position));
	}
	/* Fade before the admission boundary instead of popping at 25 units. */
	sample->strength *= PIN((25.0f - nearest) / 3.0f, 0.0f, 1.0f);
	return TRUE;
}

static void emitter_release(struct emitter_light *emitter)
{
	if (emitter->active && emitter->light != NONE &&
		light_port_glow_set(emitter->light, NONE, &emitter_black, 0.0f))
		light_delete(emitter->light);
	memset(emitter, 0, sizeof(*emitter));
}

void covenant_emitter_lighting_update(void)
{
	struct object_iterator iterator;
	int i, slot;
	real now = render_interpolation_game_time_sec(game_time_get());
	for (i = 0; i < EMITTER_LIGHTS; i++)
	{
		if (emitters[i].active && (!emitter_kind_enabled(emitters[i].kind) || now < emitters[i].last_time))
			emitter_release(&emitters[i]);
		emitters[i].seen = FALSE;
	}
	if (!covenant_emitter_lighting_enabled())
		return;
	object_iterator_new(&iterator, _object_mask_biped | _object_mask_weapon | _object_mask_scenery, 0);
	while (object_iterator_next(&iterator))
	{
		struct emitter_sample sample;
		struct emitter_light *emitter;
		real_rgb_color color;
		real blend;
		long definition;
		if (!emitter_sample(iterator.index, &sample))
			continue;
		slot = NONE;
		for (i = 0; i < EMITTER_LIGHTS; i++)
		{
			if (emitters[i].active && emitters[i].object == iterator.index)
			{
				slot = i;
				break;
			}
			if (!emitters[i].active && slot == NONE)
				slot = i;
		}
		if (slot == NONE || (!emitters[slot].active && sample.strength <= 0.0f))
			continue;
		emitter = &emitters[slot];
		if (!emitter->active)
		{
			emitter->active = TRUE;
			emitter->object = iterator.index;
			emitter->kind = sample.kind;
			emitter->light = emitter->rasterizer = NONE;
			emitter->last_time = now;
		}
		emitter->seen = TRUE;
		/* Exponential easing has the same response at 30, 60 and 120 FPS.
		   Repeated views at the same render time do not advance the fade. */
		blend = 1.0f - (real)exp(-(double)PIN(now - emitter->last_time, 0.0f, 0.25f) / 0.08);
		emitter->strength += (sample.strength - emitter->strength) * blend;
		emitter->last_time = now;
		emitter->radius = sample.radius;
		if (emitter->strength < 0.002f)
		{
			if (sample.strength <= 0.0f)
				emitter_release(emitter);
			continue;
		}
		color.red = sample.color.red * emitter->strength;
		color.green = sample.color.green * emitter->strength;
		color.blue = sample.color.blue * emitter->strength;
		if (emitter->light == NONE)
		{
			definition = light_port_glow_definition_get();
			if (definition != NONE)
				emitter->light = light_new_unattached(definition, NONE, 0, &sample.position, global_forward3d, 0.0f);
		}
		if (!light_port_glow_set(emitter->light, NONE, &color, emitter->radius) ||
			!light_port_glow_move(emitter->light, &sample.position))
		{
			emitter_release(emitter);
			continue;
		}
		emitter->connected = TRUE;
	}
	for (i = 0; i < EMITTER_LIGHTS; i++)
		if (emitters[i].active && !emitters[i].seen)
			emitter_release(&emitters[i]);
}

void covenant_emitter_lighting_frame_end(void)
{
	int i;
	for (i = 0; i < EMITTER_LIGHTS; i++)
	{
		struct emitter_light *emitter = &emitters[i];
		if (!emitter->active)
			continue;
		if (!emitter_kind_enabled(emitter->kind))
			emitter_release(emitter);
		else if (emitter->connected && light_port_glow_set(emitter->light, NONE, &emitter_black, 0.0f))
		{
			light_disconnect_from_map(emitter->light);
			emitter->connected = FALSE;
		}
		emitter->rasterizer = NONE;
	}
}

void covenant_emitter_lighting_forget(void)
{
	ammo_counter_lighting_forget();
	memset(emitters, 0, sizeof(emitters));
}

void covenant_emitter_lighting_window_begin(void)
{
	int i;
	for (i = 0; i < EMITTER_LIGHTS; i++)
		emitters[i].rasterizer = NONE;
}

void covenant_emitter_lighting_submitted(long light, long rasterizer)
{
	int i;
	for (i = 0; i < EMITTER_LIGHTS; i++)
		if (emitters[i].active && emitters[i].light == light && light != NONE)
		{
			emitters[i].rasterizer = rasterizer;
			plasma_occlusion_submit(light, rasterizer);
		}
}

int covenant_emitter_lighting_selected(long rasterizer)
{
	int i;
	if (rasterizer == NONE)
		return FALSE;
	for (i = 0; i < EMITTER_LIGHTS; i++)
		if (emitters[i].active && emitters[i].rasterizer == rasterizer && emitter_kind_enabled(emitters[i].kind))
			return TRUE;
	return FALSE;
}

float covenant_emitter_lighting_radius(long light, float radius)
{
	int i;
	for (i = 0; i < EMITTER_LIGHTS; i++)
		if (emitters[i].active && emitters[i].light == light && light != NONE && emitter_kind_enabled(emitters[i].kind))
			return MAX(radius, emitters[i].radius);
	return radius;
}

long covenant_emitter_lighting_occlusion_ignore(long light)
{
	int i;
	/* A luminous shield must not block its own point approximation. Other
	   scenery and machines still occlude it; bipeds/weapons are not blockers. */
	for (i = 0; i < EMITTER_LIGHTS; i++)
		if (emitters[i].active && emitters[i].kind == 2 && emitters[i].light == light &&
			light != NONE && emitter_kind_enabled(2))
			return emitters[i].object;
	return NONE;
}
