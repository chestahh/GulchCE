#include "harness.h"
#include <math.h>
#define BOTS_MAXIMUM 15
#define TICKS_PER_SECOND 30
#define MAXIMUM_WEAPONS_PER_UNIT 4
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define PIN(a,l,h) ((a) < (l) ? (l) : (a) > (h) ? (h) : (a))
#define csmemset memset
#define platform_log(...) ((void)0)
#define VEHICLE_DEFINITION_TAG 0
#define _object_mask_vehicle 1
enum { _object_dead_bit, _unit_not_enterable_by_player_bit, _unit_seat_driver_bit, _unit_seat_gunner_bit };
enum { _unit_control_action_bit, _unit_control_jump_bit, _unit_control_throw_grenade_bit,
	_unit_control_use_equipment_bit, _unit_control_weapon_primary_trigger_bit };
enum { _unit_nearby_seat_available = 2 };
struct object_state {
	long parent_object_index;
	real_vector3d up, forward;
	real_point3d position;
	unsigned long damage_flags;
};
struct object_datum { long definition_index; struct object_state object; };
struct unit_datum {
	long definition_index;
	struct object_state object;
	struct { long driver_object_index, gunner_object_index, weapon_object_indices[4];
		short current_weapon_index, parent_seat_index; unsigned long flags; } unit;
};
struct unit_seat { unsigned long flags; };
struct unit_definition { struct { struct { long count; struct unit_seat *address; } seats; } unit; };
#define TAG_BLOCK_GET_ELEMENT(block, i, type) (&(block)->address[i])
struct player_action { struct { real i, j; } throttle; struct { real yaw, pitch; } desired_facing;
	unsigned long control_flags; real primary_trigger; };
static struct unit_datum units[2];
static struct unit_seat seat;
static struct unit_definition tag;
static byte vehicle_tag[1024];
static long now = 100;
static int enabled = 1, available = 1;
static int config_boolean(char const *name) { (void)name; return enabled; }
static long game_time_get(void) { return now; }
static struct object_datum *object_get(long index) { CHECK(index >= 0 && index < 2, "index"); return (void *)&units[index]; }
static struct object_datum *object_try_and_get(long index) { return index >= 0 && index < 2 ? object_get(index) : NULL; }
static struct object_datum *object_try_and_get_and_verify_type(long index, int type)
{ (void)type; return index == 1 ? object_get(index) : NULL; }
static struct unit_datum *unit_get(long index) { return &units[index]; }
static void *tag_get(int group, long index) { (void)group; (void)index; return vehicle_tag; }
static struct unit_definition *unit_definition_get(long index) { (void)index; return &tag; }
static boolean unit_can_enter_seat(long a, long b, short c, void *d)
{ (void)a; (void)b; (void)c; (void)d; return available; }
static int unit_find_nearby_seat(long a, long b, short *chosen)
{ (void)a; (void)b; *chosen = 0; return available ? _unit_nearby_seat_available : 0; }
#include "under_test.inc"

int main(int argc, char **argv)
{
	struct player_action action;
	real_point3d goal = { 100, 0, 0 };
	real radius = 0.1f;
	int i;
	CHECK(argc == 2, "case");
	bots_vehicles_reset();
	memset(&action, 0, sizeof(action));
	units[0].object.parent_object_index = 1;
	units[1].unit.driver_object_index = 0;
	units[1].unit.gunner_object_index = NONE;
	units[1].object.up.k = units[1].object.forward.i = 1;
	tag.unit.seats.count = 1;
	tag.unit.seats.address = &seat;
	seat.flags = FLAG(_unit_seat_driver_bit);
	((struct bots_vehicle_definition *)vehicle_tag)->type = 1;
	((struct bots_vehicle_definition *)vehicle_tag)->radius = 1.5f;
	if (!strcmp(argv[1], "seat"))
	{
		CHECK(!seat_available(0, 0, 1, -1) && !seat_available(0, 0, 1, 1), "seat bounds");
		CHECK(seat_available(0, 0, 1, 0), "valid seat refused");
		vehicles[1].sought = 1; vehicles[1].seat = 0;
		CHECK(!seat_available(0, 0, 1, 0), "reservation ignored");
		bots_vehicles_reset_player(1);
		available = 0;
		CHECK(!seat_available(0, 0, 1, 0), "engine rejection ignored");
	}
	else if (!strcmp(argv[1], "driver"))
	{
		CHECK(bots_vehicle_navigation(0, &radius) == 1 && radius == 1.5f, "driver used biped navigation");
		action.primary_trigger = 1; action.control_flags = FLAG(_unit_control_weapon_primary_trigger_bit);
		bots_vehicle_action(0, 0, &goal, FALSE, TRUE, 0.7f, &action);
		CHECK(action.throttle.i > 0 && action.desired_facing.yaw == 0.7f, "driver did not steer");
		CHECK(!action.primary_trigger && !TEST_FLAG(action.control_flags, _unit_control_weapon_primary_trigger_bit), "steering fired an aimed shot");
	}
	else if (!strcmp(argv[1], "disable"))
	{
		enabled = 0;
		bots_vehicle_action(0, 0, &goal, FALSE, FALSE, 0, &action);
		CHECK(TEST_FLAG(action.control_flags, _unit_control_action_bit), "disable left seated bot stranded");
		units[0].object.parent_object_index = NONE;
		bots_vehicle_action(0, 0, &goal, FALSE, FALSE, 0, &action);
		CHECK(vehicles[0].ridden == NONE && vehicles[0].sought == NONE, "exit retained reservation");
	}
	else if (!strcmp(argv[1], "gunner"))
	{
		units[1].unit.driver_object_index = NONE; units[1].unit.gunner_object_index = 0;
		units[1].unit.current_weapon_index = -1;
		CHECK(bots_vehicle_weapon(0, 90) == 90, "invalid weapon slot");
		units[1].unit.current_weapon_index = 2; units[1].unit.weapon_object_indices[2] = 91;
		CHECK(bots_vehicle_weapon(0, 90) == 91, "gunner did not use turret weapon");
		action.primary_trigger = 1;
		bots_vehicle_action(0, 0, &goal, FALSE, FALSE, 0, &action);
		CHECK(action.primary_trigger == 1, "gunner trigger cleared");
	}
	else if (!strcmp(argv[1], "stuck"))
	{
		for (i = 0; i <= 6; i++) {
			now += TICKS_PER_SECOND;
			action.control_flags = 0;
			bots_vehicle_action(0, 0, &goal, FALSE, TRUE, 0, &action);
		}
		CHECK(TEST_FLAG(action.control_flags, _unit_control_action_bit), "stuck driver never exited");
	}
	else CHECK(0, "unknown case");
	return 0;
}
