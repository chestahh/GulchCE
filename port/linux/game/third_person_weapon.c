/* Offline muzzle-origin firing. Network games retain their agreed firing
   rules until a host-authoritative shoulder-mode setting is implemented. */
#include "cseries.h"
#include "third_person.h"
#include "game/game.h"
#include "game/players.h"
#include "units/units.h"
#include "physics/collisions.h"

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

static boolean muzzle_position(long weapon_index, long unit_index, short trigger_index,
	real_point3d *position)
{
	struct object_marker marker;
	real_point3d eye;
	real_vector3d delta;
	real length;
	struct collision_result collision;
	if (weapon_index == NONE || trigger_index < 0 || trigger_index > 1 ||
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
		point_from_line3d(&eye, &delta, MAX(0.0f, collision.t - 0.01f / length), position);
	return TRUE;
}

void third_person_muzzle_shot(long weapon_index, long unit_index, short trigger_index,
	real_point3d *origin, real_vector3d *forward)
{
	short player;
	real_point3d muzzle, target;
	real_vector3d ray;
	struct collision_result collision;
	for (player = 0; player < MAXIMUM_LOCAL_PLAYERS; player++)
		if (muzzle_player(player, unit_index))
			break;
	if (player == MAXIMUM_LOCAL_PLAYERS || !muzzle_position(weapon_index, unit_index, trigger_index, &muzzle))
		return;
	/* Retain the aim-assist result; weapon spread is applied by the caller
	   afterwards. Converge on its first hit, not an arbitrary fixed range. */
	scale_vector3d(forward, 1000.0f, &ray);
	point_from_line3d(origin, &ray, 1.0f, &target);
	if (collision_test_vector(_collision_test_for_projectiles_flags, origin, &ray, unit_index, &collision))
		target = collision.point;
	vector_from_points3d(&muzzle, &target, &ray);
	if (normalize3d(&ray) > 0.001f)
	{
		*origin = muzzle;
		*forward = ray;
	}
}

void third_person_muzzle_target(short player, real_point3d *target)
{
	long unit_index, weapon_index;
	struct unit_datum *unit;
	real_point3d muzzle;
	real_vector3d ray;
	struct collision_result collision;
	unit_index = player_control_get_unit_index(player);
	if (!muzzle_player(player, unit_index))
		return;
	unit = unit_get(unit_index);
	weapon_index = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);
	if (!muzzle_position(weapon_index, unit_index, 0, &muzzle))
		return;
	vector_from_points3d(&muzzle, target, &ray);
	if (collision_test_vector(_collision_test_for_projectiles_flags, &muzzle, &ray, unit_index, &collision))
		*target = collision.point;
}
