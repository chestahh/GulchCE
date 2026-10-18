/* Enhanced energy shields. Only the owning unit's modifier is eligible;
ordinary plasma materials retain their original shaders. The transparent
queue keeps the original animation pointers, so no per-unit history or
extra lifetime is needed for sorted, mirrored or split-screen draws. */

#include "cseries.h"
#include "real_math.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "shaders/shader_definitions.h"
#include "shaders/shaders.h"
#include "shield_look.h"

#include <string.h>

const char *config_string(const char *name);
unsigned long config_changes(void);

int shield_look_enabled(void)
{
	static unsigned long changes = (unsigned long)-1;
	static int enabled;
	const char *value;

	if (changes != config_changes())
	{
		changes = config_changes();
		value = config_string("display.shield_look");
		enabled = value && !strcmp(value, "enhanced");
	}
	return enabled;
}

static struct unit_datum *shield_look_unit(long object_index,
	struct shader const *shader, float const *values, int first_person)
{
	struct object_datum *object;
	struct object_definition *definition;

	if (!shield_look_enabled() || !values || !shader || shader->base.type != 10)
		return NULL;
	object = object_try_and_get(object_index);
	/* The arms carry the held weapon's draw identifier, but the unit's
	modifier animation. The weapon itself has different animation values. */
	if (object && first_person && object->object.type == _object_type_weapon)
		object = object_try_and_get(object->object.parent_object_index);
	if (!object || !TEST_FLAG(_object_mask_unit, object->object.type) ||
		object->object.maximum_shield_vitality <= 0.f ||
		TEST_FLAG(object->object.flags, _object_invisible_bit) ||
		TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||
		values != object->object.outgoing_function_values)
		return NULL;
	definition = object_definition_get(object->definition_index);
	if (!definition || definition->object.modifier_shader.index == NONE ||
		shader != shader_definition_get(definition->object.modifier_shader.index))
		return NULL;
	return (struct unit_datum *)object;
}

int shield_look_eligible(long object_index, struct shader const *shader,
	float const *values, int first_person)
{
	return shield_look_unit(object_index, shader, values, first_person) != NULL;
}

int shield_look_visible(long object_index, struct shader const *shader,
	float const *values, int first_person)
{
	struct unit_datum *unit = shield_look_unit(object_index, shader, values, first_person);

	return unit && unit->unit.active_camouflage < 1.f &&
		(TEST_FLAG(unit->object.damage_flags, _object_shield_depleted_bit) ||
		 TEST_FLAG(unit->object.damage_flags, _object_shield_charging_bit));
}

void shield_look_begin(long object_index, struct shader const *shader,
	float const *values, int first_person, float time, float intensity,
	float const *color, float const *tint_color, float parallel_alpha, float perpendicular_alpha)
{
	struct unit_datum *unit = shield_look_unit(object_index, shader, values, first_person);
	float state[4];
	float tint[4];
	float rim[4];

	shield_look_end();
	if (!unit)
		return;
	state[0] = time;
	state[1] = PIN(intensity, 0.f, 1.f);
	state[2] = TEST_FLAG(unit->object.damage_flags, _object_shield_charging_bit) ?
		1.f - PIN(unit->object.shield_vitality, 0.f, 1.f) : 0.f;
	state[3] = TEST_FLAG(unit->object.damage_flags, _object_shield_depleted_bit) ? 1.f : 0.f;
	tint[0] = color[0] * tint_color[0];
	tint[1] = color[1] * tint_color[1];
	tint[2] = color[2] * tint_color[2];
	tint[3] = 1.f - PIN(unit->unit.active_camouflage, 0.f, 1.f);
	rim[0] = parallel_alpha;
	rim[1] = perpendicular_alpha;
	rim[2] = intensity;
	rim[3] = first_person ? 1.f : 0.f;
	shield_look_set(state, tint, rim);
}

void shield_look_end(void)
{
	shield_look_set(NULL, NULL, NULL);
}
