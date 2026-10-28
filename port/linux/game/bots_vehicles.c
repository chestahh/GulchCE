/* Focused adaptation of BirchWoodGod/halo-ce-vita's ground-vehicle bots,
via thelinkin3000/halo-ce-universal d92cc93385edf03d0a4a21cfe4c58dd578a3e786.
The inherited vehicle definitions and seat checks are GPL-3.0-only.
See docs/reading/bots_and_android_features.md for provenance.

The base bots own paths and combat. This module chooses an accessible seat,
uses the action button to enter/leave, and translates a path direction into
driving controls. No new attachment, vehicle or network rules are needed. */
#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "units/vehicle_definitions.h"
#include "tag_files/tag_groups.h"
#include "bots.h"
#include "bots_tactics.h"
#include "bots_vehicles.h"
#include <math.h>
#include <stddef.h>

int config_boolean(char const *name);

/* The public vehicle tag layout used by the fork; no SDK declarations. */
struct bots_vehicle_definition
{
	byte unit[0x2F4];
	short type;
	short pad;
	byte unknown[0x94];
	real radius;
};
typedef char bots_vehicle_radius_offset[offsetof(struct bots_vehicle_definition, radius) == 0x38C ? 1 : -1];

static struct bots_vehicle_state
{
	long sought, ridden, avoided;
	short seat;
	long next_scan, deadline, next_press, stuck_tick, reverse_until, stuck_count;
	real_point3d entrance, last_position;
} vehicles[BOTS_MAXIMUM];

static real distance(real_point3d const *a, real_point3d const *b)
{
	real x = a->x - b->x, y = a->y - b->y, z = a->z - b->z;
	return (real)sqrt(x*x + y*y + z*z);
}

static real angle(real value)
{
	value = (real)fmod(value, 6.28318531f);
	if (value > 3.14159265f) value -= 6.28318531f;
	if (value < -3.14159265f) value += 6.28318531f;
	return value;
}

static struct bots_vehicle_definition *definition(long index)
{
	return (struct bots_vehicle_definition *)tag_get(VEHICLE_DEFINITION_TAG, object_get(index)->definition_index);
}

static boolean drivable(long index)
{
	short type = definition(index)->type;
	/* The engine's human tank, human jeep and alien scout tag values. */
	return type == 0 || type == 1 || type == 3;
}

void bots_vehicles_reset_player(long index)
{
	csmemset(&vehicles[index], 0, sizeof(vehicles[index]));
	vehicles[index].sought = vehicles[index].ridden = vehicles[index].avoided = NONE;
	vehicles[index].seat = NONE;
}

void bots_vehicles_reset(void)
{
	long index;
	for (index = 0; index < BOTS_MAXIMUM; index++)
		bots_vehicles_reset_player(index);
}

long bots_vehicle_parent(long unit_index)
{
	struct object_datum *object = object_try_and_get(unit_index);
	long parent = object ? object->object.parent_object_index : NONE;
	return parent != NONE && object_try_and_get_and_verify_type(parent, _object_mask_vehicle) ? parent : NONE;
}

long bots_vehicle_navigation(long unit_index, real *radius)
{
	long parent = bots_vehicle_parent(unit_index);
	if (parent != NONE && unit_get(parent)->unit.driver_object_index == unit_index && drivable(parent))
	{
		real width = definition(parent)->radius;
		*radius = width > 0.1f ? MIN(width, 2.0f) : 1.0f;
		return parent;
	}
	return unit_index;
}

long bots_vehicle_weapon(long unit_index, long weapon_index)
{
	long parent = bots_vehicle_parent(unit_index);
	struct unit_datum *vehicle;
	short slot;
	if (parent == NONE) return weapon_index;
	vehicle = unit_get(parent);
	slot = vehicle->unit.current_weapon_index;
	if (vehicle->unit.gunner_object_index == unit_index && slot >= 0 && slot < MAXIMUM_WEAPONS_PER_UNIT)
		return vehicle->unit.weapon_object_indices[slot];
	return weapon_index;
}

static boolean seat_available(long index, long unit_index, long vehicle_index, short seat_index)
{
	struct unit_datum *vehicle = unit_get(vehicle_index);
	struct unit_definition *tag = unit_definition_get(vehicle->definition_index);
	struct unit_seat *seat;
	long other;
	if (seat_index < 0 || seat_index >= tag->unit.seats.count ||
		vehicle->object.up.k < 0.5f || TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit) ||
		TEST_FLAG(vehicle->unit.flags, _unit_not_enterable_by_player_bit)) return FALSE;
	seat = TAG_BLOCK_GET_ELEMENT(&tag->unit.seats, seat_index, struct unit_seat);
	if (TEST_FLAG(seat->flags, _unit_seat_driver_bit) && !drivable(vehicle_index)) return FALSE;
	if (!TEST_FLAG(seat->flags, _unit_seat_driver_bit) && !TEST_FLAG(seat->flags, _unit_seat_gunner_bit) &&
		vehicle->unit.driver_object_index == NONE) return FALSE;
	/* A seat marked as requiring a driver follows the engine's player check. */
	if (TEST_FLAG(seat->flags, 9) && vehicle->unit.driver_object_index == NONE) return FALSE;
	for (other = 0; other < BOTS_MAXIMUM; other++)
		if (other != index && vehicles[other].sought == vehicle_index && vehicles[other].seat == seat_index)
			return FALSE;
	return unit_can_enter_seat(unit_index, vehicle_index, seat_index, NULL);
}

boolean bots_vehicle_goal(long index, long unit_index, real_point3d const *destination,
	real_point3d *goal, long *surface)
{
	struct bots_vehicle_state *state = &vehicles[index];
	struct object_datum *unit = object_get(unit_index);
	long now = game_time_get();
	if (!config_boolean("bots.vehicles") || bots_vehicle_parent(unit_index) != NONE) return FALSE;
	if (state->sought != NONE && (!object_try_and_get_and_verify_type(state->sought, _object_mask_vehicle) ||
		now >= state->deadline || !seat_available(index, unit_index, state->sought, state->seat)))
	{
		state->avoided = state->sought;
		state->sought = NONE;
		state->next_scan = now + TICKS_PER_SECOND * 10;
	}
	if (state->sought == NONE && now >= state->next_scan)
	{
		struct object_iterator iterator;
		struct object_datum *vehicle;
		real best = 25.0f;
		state->next_scan = now + TICKS_PER_SECOND * 2;
		object_iterator_new(&iterator, _object_mask_vehicle, 0);
		while ((vehicle = object_iterator_next(&iterator)) != NULL)
		{
			struct unit_definition *tag;
			short seat;
			real reach = distance(&unit->object.position, &vehicle->object.position);
			if (iterator.index == state->avoided || reach >= best ||
				distance(&vehicle->object.position, destination) > distance(&unit->object.position, destination) + 8.0f)
				continue;
			tag = unit_definition_get(vehicle->definition_index);
			for (seat = 0; seat < tag->unit.seats.count; seat++)
			{
				real_point3d entrance, seat_point;
				if (seat_available(index, unit_index, iterator.index, seat) &&
					unit_get_seat_entrance_point(unit_index, iterator.index, seat, &entrance, &seat_point, NULL))
				{
					best = reach;
					state->sought = iterator.index;
					state->seat = seat;
					state->entrance = entrance;
					state->deadline = now + TICKS_PER_SECOND * 15;
					break;
				}
			}
		}
	}
	if (state->sought == NONE) return FALSE;
	*surface = bots_routes_ground(&state->entrance, goal);
	return *surface != NONE;
}

void bots_vehicle_action(long index, long unit_index, real_point3d const *goal,
	boolean objective, boolean moving, real direction, struct player_action *action)
{
	struct bots_vehicle_state *state = &vehicles[index];
	long parent = bots_vehicle_parent(unit_index), now = game_time_get();
	struct unit_datum *vehicle;
	if (parent == NONE)
	{
		if (state->ridden != NONE)
		{
			state->avoided = state->ridden;
			state->ridden = state->sought = NONE;
			state->next_scan = now + TICKS_PER_SECOND * 15;
		}
		if (config_boolean("bots.vehicles") && state->sought != NONE && now >= state->next_press &&
			object_try_and_get_and_verify_type(state->sought, _object_mask_vehicle))
		{
			short nearest;
			/* Press Use only for the chosen seat; the normal player code
			checks reach, motion, hostility and occupancy again. */
			if (unit_find_nearby_seat(unit_index, state->sought, &nearest) == _unit_nearby_seat_available && nearest == state->seat)
			{
				action->throttle.i = action->throttle.j = 0.0f;
				action->control_flags |= FLAG(_unit_control_action_bit);
				state->next_press = now + TICKS_PER_SECOND / 2;
			}
			else if (distance(&object_get(unit_index)->object.position, &state->entrance) < 3.0f)
			{
				real_point3d *position = &object_get(unit_index)->object.position;
				real toward = (real)atan2(state->entrance.y - position->y, state->entrance.x - position->x);
				real relative = angle(toward - action->desired_facing.yaw);
				action->throttle.i = (real)cos(relative) * 0.5f;
				action->throttle.j = (real)sin(relative) * 0.5f;
			}
		}
		return;
	}
	vehicle = unit_get(parent);
	if (state->ridden != parent)
	{
		state->ridden = parent;
		state->sought = NONE;
		state->stuck_tick = now;
		state->last_position = vehicle->object.position;
		state->stuck_count = state->reverse_until = 0;
		platform_log("bots: bot %ld entered vehicle %ld seat %d", index + 1, parent, unit_get(unit_index)->unit.parent_seat_index);
	}
	action->control_flags &= ~(FLAG(_unit_control_jump_bit) | FLAG(_unit_control_throw_grenade_bit) |
		FLAG(_unit_control_use_equipment_bit));
	action->throttle.i = action->throttle.j = 0.0f;
	if (vehicle->unit.driver_object_index == unit_index && drivable(parent))
	{
		real heading = (real)atan2(vehicle->object.forward.j, vehicle->object.forward.i);
		if (now - state->stuck_tick >= TICKS_PER_SECOND)
		{
			if (moving && distance(&state->last_position, &vehicle->object.position) < 0.6f)
			{
				state->stuck_count++;
				if (state->stuck_count % 2 == 0) state->reverse_until = now + TICKS_PER_SECOND;
			}
			else state->stuck_count = 0;
			state->last_position = vehicle->object.position;
			state->stuck_tick = now;
		}
		if (moving)
		{
			action->desired_facing.yaw = direction;
			action->desired_facing.pitch = 0.0f;
			action->throttle.i = fabs(angle(direction - heading)) > 1.57f ? 0.6f : 1.0f;
			if (now < state->reverse_until)
			{
				action->throttle.i = -0.6f;
				action->desired_facing.yaw = heading - PIN(angle(direction - heading), -1.0f, 1.0f);
			}
		}
		/* Changing the driver's aim must not turn a shot at an enemy into
		a shot down the route. Gunners retain their separate combat controls. */
		if (fabs(angle(action->desired_facing.yaw - direction)) > 0.15f || moving)
		{
			action->control_flags &= ~FLAG(_unit_control_weapon_primary_trigger_bit);
			action->primary_trigger = 0.0f;
		}
	}
	if ((!config_boolean("bots.vehicles") || vehicle->object.up.k < 0.3f || state->stuck_count >= 6 ||
		(objective && distance(goal, &vehicle->object.position) < 6.0f)) && now >= state->next_press)
	{
		action->control_flags |= FLAG(_unit_control_action_bit);
		state->next_press = now + TICKS_PER_SECOND / 2;
	}
}
