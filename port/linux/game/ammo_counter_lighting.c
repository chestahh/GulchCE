/* The AR display is luminous in both models. Sample its rendered pose only;
   the emitter module owns allocation, fading and frame cleanup. */
#include "cseries.h"
#include "objects/objects.h"
#include "items/weapons.h"
#include "units/units.h"
#include "interface/first_person_weapons.h"
#include "ammo_counter_lighting.h"
#include <string.h>

int config_boolean(const char *name);

int ammo_counter_lighting_position(long index, real_point3d *position)
{
	struct weapon_datum *weapon;
	struct unit_datum *unit;
	struct object_marker marker;
	real_vector3d forward, up;
	real height;
	const char *name;
	if (!config_boolean("display.ammo_counter_lighting"))
		return FALSE;
	weapon = weapon_try_and_get(index);
	if (!weapon || TEST_FLAG(weapon->object.flags, _object_invisible_bit))
		return FALSE;
	name = tag_get_name(weapon->definition_index);
	if (!name || strcmp(name, "weapons\\assault rifle\\assault rifle"))
		return FALSE;
	unit = unit_try_and_get(weapon->object.parent_object_index);
	if (!unit || TEST_FLAG(unit->object.flags, _object_invisible_bit) ||
		unit->unit.current_weapon_index < 0 || unit->unit.current_weapon_index >= MAXIMUM_WEAPONS_PER_UNIT ||
		unit_inventory_get_weapon(weapon->object.parent_object_index, unit->unit.current_weapon_index) != index)
		return FALSE;
	/* Both stock displays lie near x=.047, z=.069 in model space. Place the
	   light just behind/above that surface, using the identity-oriented muzzle
	   marker. The FP muzzle is .002744 units higher than the world muzzle. */
	if (first_person_weapon_adjust_light(index, "primary trigger", position, &forward, &up))
		height = 0.025f;
	else
	{
		if (weapon->object.node_matrices.size < sizeof(real_matrix4x3) ||
			!object_get_marker_by_name(index, "primary trigger", &marker, 1))
			return FALSE;
		*position = marker.matrix.position;
		forward = marker.matrix.forward;
		up = marker.matrix.up;
		height = 0.027744f;
	}
	position->x += -0.18f * forward.i + height * up.i;
	position->y += -0.18f * forward.j + height * up.j;
	position->z += -0.18f * forward.k + height * up.k;
	return TRUE;
}
