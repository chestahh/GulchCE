/* Offline muzzle-origin firing. Network games retain their agreed firing
   rules until a host-authoritative shoulder-mode setting is implemented. */
#include "cseries.h"
#include "third_person.h"
#include "game/game.h"
#include "game/players.h"
#include "units/units.h"
#include "physics/collisions.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "units/unit_definitions.h"
#include "effects/effect_definitions.h"

static boolean muzzle_player(short player, long unit_index)
{
	struct unit_datum *unit;
	if (game_connection() != _game_connection_local || !third_person_active(player) ||
		unit_index == NONE || player_control_get_unit_index(player) != unit_index ||
		player_control_get_zoom_level(player) != NONE)
		return FALSE;
	unit = unit_try_and_get(unit_index);
	return unit && unit->object.parent_object_index == NONE &&
		!TEST_FLAG(unit->object.damage_flags, _object_dead_bit);
}

/* Authored alternating firing effects can name a different barrel, as
   the Plasma Rifle does. Only accept trigger markers, never smoke/ejection
   locations; missing or custom names retain the ordinary trigger marker. */
static char const *muzzle_marker(long weapon_index, short trigger_index)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct weapon_definition *definition = weapon_definition_get(weapon->definition_index);
	struct weapon_trigger_definition *trigger;
	struct weapon_firing_effect_definition *firing;
	struct effect_definition *effect;
	char const *name;
	char const *fallback = trigger_index ? "secondary trigger" : "primary trigger";
	short index = weapon->weapon.triggers[trigger_index].firing_effect_index;
	if (definition->weapon.triggers.count <= trigger_index)
		return fallback;
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger_index, struct weapon_trigger_definition);
	if (index < 0 || index >= trigger->firing_effects.count)
		return fallback;
	firing = TAG_BLOCK_GET_ELEMENT(&trigger->firing_effects, index, struct weapon_firing_effect_definition);
	if (firing->effects[0].index == NONE)
		return fallback;
	effect = (struct effect_definition *)tag_get(EFFECT_DEFINITION_TAG, firing->effects[0].index);
	if (!effect->locations.count)
		return fallback;
	/* Effect locations are 32-byte marker names. The tag loader validates
	   this block, and object_get_marker_by_name still verifies the marker. */
	name = (char const *)effect->locations.address;
	if (!strncmp(name, fallback, strlen(fallback)) &&
		(name[strlen(fallback)] == '\0' ||
		 (name[strlen(fallback)] >= '0' && name[strlen(fallback)] <= '9')))
		return name;
	return fallback;
}

static boolean muzzle_position(long weapon_index, long unit_index, short trigger_index,
	real_point3d *position, real_point3d *obstruction, boolean *blocked)
{
	struct object_marker marker;
	real_point3d eye;
	real_vector3d delta;
	real length;
	struct collision_result collision;
	*blocked = FALSE;
	if (weapon_index == NONE || trigger_index < 0 || trigger_index > 1)
		return FALSE;
	if (!object_get_marker_by_name(weapon_index, muzzle_marker(weapon_index, trigger_index), &marker, 1) &&
		!object_get_marker_by_name(weapon_index,
			trigger_index ? "secondary trigger" : "primary trigger", &marker, 1))
		return FALSE;
	unit_get_camera_position(unit_index, &eye);
	*position = marker.matrix.position;
	vector_from_points3d(&eye, position, &delta);
	length = magnitude3d(&delta);
	/* Do not let a barrel through a wall spawn a shot beyond that wall. */
	if (length > 0.001f && collision_test_vector(_collision_test_for_projectiles_flags,
		&eye, &delta, unit_index, &collision))
	{
		point_from_line3d(&eye, &delta, MAX(0.0f, collision.t - 0.01f / length), position);
		*obstruction = collision.point;
		*blocked = TRUE;
	}
	return TRUE;
}

/* Both the preview and actual shot use this solver. The input ray is the
   stock weapon ray (aim assistance has already run for a real shot). */
static boolean muzzle_aim(long weapon_index, long unit_index, short trigger_index,
	real_point3d *origin, real_vector3d *forward, real_point3d *target)
{
	real_point3d muzzle, eye, obstruction;
	real_vector3d ray;
	boolean blocked;
	struct collision_result collision;
	if (!muzzle_position(weapon_index, unit_index, trigger_index, &muzzle, &obstruction, &blocked))
		return FALSE;
	scale_vector3d(forward, 1000.0f, &ray);
	point_from_line3d(origin, &ray, 1.0f, target);
	if (collision_test_vector(_collision_test_for_projectiles_flags, origin, &ray, unit_index, &collision))
		*target = collision.point;
	/* The stock offset can start beyond nearby cover. Check the segment
	   leading to that origin as well, rather than aiming through the wall. */
	unit_get_camera_position(unit_index, &eye);
	vector_from_points3d(&eye, origin, &ray);
	if (magnitude3d(&ray) > 0.001f && collision_test_vector(_collision_test_for_projectiles_flags,
		&eye, &ray, unit_index, &collision))
		*target = collision.point;
	if (blocked)
		*target = obstruction;
	vector_from_points3d(&muzzle, target, &ray);
	/* A target behind the barrel must not turn the shot backwards. Keep
	   its original direction; the preview traces that same forward path. */
	if ((blocked || dot_product3d(&ray, forward) > 0.001f) && normalize3d(&ray) > 0.001f)
		*forward = ray;
	*origin = muzzle;
	scale_vector3d(forward, 1000.0f, &ray);
	point_from_line3d(origin, &ray, 1.0f, target);
	if (collision_test_vector(_collision_test_for_projectiles_flags, origin, &ray, unit_index, &collision))
		*target = collision.point;
	return TRUE;
}

boolean third_person_muzzle_shot(long weapon_index, long unit_index, short trigger_index,
	real_point3d *origin, real_vector3d *forward)
{
	short player;
	real_point3d target;
	for (player = 0; player < MAXIMUM_LOCAL_PLAYERS; player++)
		if (muzzle_player(player, unit_index))
			return muzzle_aim(weapon_index, unit_index, trigger_index, origin, forward, &target);
	return FALSE;
}

void third_person_muzzle_target(short player, real_point3d *target)
{
	long unit_index, weapon_index;
	short trigger_index = 0;
	struct unit_datum *unit;
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct weapon_trigger_definition *trigger;
	struct object_marker marker;
	real_point3d origin;
	real_vector3d forward, right, up;
	real velocity;
	unit_index = player_control_get_unit_index(player);
	if (!muzzle_player(player, unit_index))
		return;
	unit = unit_get(unit_index);
	weapon_index = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);
	if (weapon_index == NONE)
		return;
	weapon = weapon_get(weapon_index);
	definition = weapon_definition_get(weapon->definition_index);
	/* A charged secondary shot leaves a different muzzle on the pistol. */
	if (definition->weapon.triggers.count > 1 &&
		((weapon->weapon.triggers[0].state == _trigger_charged &&
		  TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, 0, struct weapon_trigger_definition)->spew_time <= 0.0f) ||
		 weapon->weapon.triggers[1].state == _trigger_charging ||
		 weapon->weapon.triggers[1].state == _trigger_charged ||
		 weapon->weapon.triggers[1].state == _trigger_spewing))
		trigger_index = 1;
	if (definition->weapon.triggers.count <= trigger_index)
		return;
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger_index, struct weapon_trigger_definition);
	/* Preserve explicit custom firing rules, as the actual shot does. */
	if (TEST_FLAG(trigger->flags, _weapon_trigger_uses_weapon_origin_bit) ||
		TEST_FLAG(trigger->flags, _weapon_trigger_projectiles_cannot_be_aimed_bit))
		return;
	if (!object_get_marker_by_name(weapon_index,
		trigger_index ? "secondary trigger" : "primary trigger", &marker, 1))
		return;
	origin = marker.matrix.position;
	forward = unit->unit.aiming_vector;
	unit_adjust_projectile_ray(unit_index, &origin, &forward, &velocity,
		TEST_FLAG(unit_definition_get(unit->definition_index)->unit.flags, _unit_fires_from_camera_bit), TRUE);
	cross_product3d(global_up3d, &forward, &right);
	if (normalize3d(&right) == 0.0f)
		right = *global_left3d;
	cross_product3d(&forward, &right, &up);
	normalize3d(&up);
	point_from_line3d(&origin, &forward, trigger->first_person_weapon_offset.x, &origin);
	point_from_line3d(&origin, &right, trigger->first_person_weapon_offset.y, &origin);
	point_from_line3d(&origin, &up, trigger->first_person_weapon_offset.z, &origin);
	muzzle_aim(weapon_index, unit_index, trigger_index, &origin, &forward, target);
}
