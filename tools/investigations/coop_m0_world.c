/* Private diagnostic copy only. One isolated map session per process. */
#include "hs/hs_scenario_definitions.h"
#include "physics/collisions.h"
#include "main/main.h"
extern struct data_array *hs_syntax_data;
extern void platform_screenshot_request(void);
extern void m0_world_vehicle(boolean enter);

/* Fixture-selected device only. Rays measure collision, not player traversal. */
static void m0_world_device_rays(long tick, long index, struct object_datum *object)
{
	char const *selected = getenv("M0_WORLD_DEVICE");
	struct scenario *scenario = global_scenario_get();
	struct scenario_object_name *name;
	short axis, height;
	if (!selected || object->object.name_index < 0 ||
		object->object.name_index >= scenario->object_names.count) return;
	name = TAG_BLOCK_GET_ELEMENT(&scenario->object_names, object->object.name_index, struct scenario_object_name);
	if (strncmp(selected, name->name, 32)) return;
	for (axis = 0; axis < 2; axis++)
		for (height = 1; height <= 3; height++)
		{
			real_vector3d direction = object->object.forward;
			real_point3d origin = object->object.position;
			struct collision_result hit;
			boolean blocked;
			if (axis) { direction.i = -object->object.forward.j; direction.j = object->object.forward.i; direction.k = 0; }
			origin.x -= direction.i * 2;
			origin.y -= direction.j * 2;
			origin.z += height * 0.5f - direction.k * 2;
			direction.i *= 4; direction.j *= 4; direction.k *= 4;
			blocked = collision_test_vector(_collision_test_for_bipeds_living_flags, &origin, &direction, NONE, &hit);
			m0_emit("world_device_ray", tick, index, axis, height, blocked,
				blocked && hit.type == _collision_result_object && hit.object_index == index);
		}
}

static void m0_world_metadata(void)
{
	struct scenario *scenario = global_scenario_get();
	struct data_iterator iterator;
	struct hs_syntax_node *node;
	FILE *file = fopen("m0-world-metadata.tsv", "w");
	long index;
	if (!file) return;
	fprintf(file, "bsp_count\t%ld\n", scenario->structure_bsp_references.count);
	for (index = 0; index < scenario->hs_scripts.count; index++)
	{
		struct hs_script *script = TAG_BLOCK_GET_ELEMENT(&scenario->hs_scripts, index, struct hs_script);
		fprintf(file, "script\t%ld\t%.32s\t%d\t%d\t%ld\n", index, script->name,
			script->script_type, script->return_type, script->root_expression_index);
	}
	for (index = 0; index < scenario->object_names.count; index++)
	{
		struct scenario_object_name *name = TAG_BLOCK_GET_ELEMENT(&scenario->object_names, index, struct scenario_object_name);
		fprintf(file, "name\t%ld\t%.32s\t%d\t%d\n", index, name->name, name->runtime_object_type, name->runtime_scenario_datum_index);
	}
	for (index = 0; index < scenario->cutscene_flags.count; index++)
	{
		struct scenario_cutscene_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->cutscene_flags, index, struct scenario_cutscene_flag);
		fprintf(file, "flag\t%ld\t%.32s\t%.9g\t%.9g\t%.9g\n", index, flag->name, flag->position.x, flag->position.y, flag->position.z);
	}
	for (index = 0; index < scenario->trigger_volumes.count; index++)
	{
		struct scenario_trigger_volume *volume = TAG_BLOCK_GET_ELEMENT(&scenario->trigger_volumes, index, struct scenario_trigger_volume);
		fprintf(file, "volume\t%ld\t%.32s\t%.9g\t%.9g\t%.9g\n", index, volume->name,
			volume->position.x, volume->position.y, volume->position.z);
	}
	data_iterator_new(&iterator, hs_syntax_data);
	while ((node = data_iterator_next(&iterator)) != NULL)
		fprintf(file, "node\t%ld\t%d\t%d\t%d\t%ld\t%ld\t%ld\n", iterator.datum_index,
			node->index, node->type, node->flags, node->next_node_index, node->string_offset, node->data);
	fclose(file);
	/* Loaded and validated tag data; retain locally, never ship retail scripts. */
	if (scenario->hs_string_constants.size > 0 && scenario->hs_string_constants.size <= 16 * 1024 * 1024)
	{
		file = fopen("m0-world-strings.bin", "wb");
		if (file)
		{
			fwrite(scenario->hs_string_constants.address, 1, scenario->hs_string_constants.size, file);
			fclose(file);
		}
	}
}

static void m0_world_capture(void)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long tick = game_time_get();
	m0_emit("world_begin", tick, global_structure_bsp_index_get(), coop_host(), cinematic_in_progress(), player_input_enabled(), 0);
	object_iterator_new(&iterator, _object_mask_unit | _object_mask_projectile | _object_mask_device, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		m0_emit("world_object", tick, iterator.index, object->object.type, object->object.name_index,
			object->object.parent_object_index, object->object.location.cluster_index);
		m0_emit("world_pose", tick, iterator.index, object->object.position.x, object->object.position.y,
			object->object.position.z, object->object.damage_flags);
		m0_emit("world_health", tick, iterator.index, object->object.body_vitality, object->object.shield_vitality,
			object->object.flags, 0);
		if (TEST_FLAG(_object_mask_unit, object->object.type))
		{
			struct unit_datum *unit = unit_get(iterator.index);
			m0_emit("world_control", tick, iterator.index, unit->unit.player_index, unit->unit.actor_index,
				unit->unit.control_flags, unit->unit.parent_seat_index);
			m0_emit("world_throttle", tick, iterator.index, unit->unit.throttle.i, unit->unit.throttle.j,
				unit->unit.throttle.k, unit->unit.animation.state);
			if (unit->unit.player_index != NONE)
			{
				real_point3d origin;
				real_vector3d down = {0, 0, -3};
				struct collision_result hit;
				boolean supported;
				object_get_origin(iterator.index, &origin);
				origin.z += 0.25f;
				supported = collision_test_vector(_collision_test_for_bipeds_living_flags, &origin, &down, iterator.index, &hit);
				m0_emit("world_floor", tick, iterator.index, supported, supported ? hit.type : NONE,
					supported && hit.type == _collision_result_object ? hit.object_index : NONE, supported ? hit.point.z : 0);
			}
		}
		if (TEST_FLAG(_object_mask_device, object->object.type))
		{
			struct device_datum *device = device_get(iterator.index);
			m0_emit("world_device", tick, iterator.index, device->device.power, device->device.position,
				device->device.power_group_index, device->device.position_group_index);
			m0_world_device_rays(tick, iterator.index, object);
		}
	}
	m0_emit("world_end", tick, 0, 0, 0, 0, 0);
}

/* Direct engine controls avoid compiling a new script after the saved image.
 * Authored scripts and the normal save/revert implementation remain enabled. */
static void m0_world_command(char const *command)
{
	if (!strcmp(command, "@save")) main_save_map_nonsafe();
	else if (!strcmp(command, "@revert")) main_revert_map();
	else if (!strcmp(command, "@drive")) m0_world_vehicle(TRUE);
	else if (!strcmp(command, "@exit")) m0_world_vehicle(FALSE);
	else if (!strcmp(command, "@move"))
	{
		struct data_iterator iterator;
		struct player_datum *player;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL)
			if (player->unit_index != NONE)
			{
				real_point3d point;
				object_get_origin(player->unit_index, &point);
				point.x += 3;
				player_teleport(iterator.datum_index, player->unit_index, &point);
				break;
			}
	}
	else if (!strcmp(command, "@open") || !strcmp(command, "@close"))
	{
		char const *name = getenv("M0_WORLD_DEVICE");
		short name_index = name ? scenario_object_name_index_from_string(global_scenario_get(), name) : NONE;
		long index = name_index != NONE ? object_index_from_name_index(name_index) : NONE;
		struct object_datum *object = object_try_and_get_and_verify_type(index, _object_mask_device);
		if (object)
		{
			device_set_power(index, 1);
			device_set_desired_position(index, !strcmp(command, "@open") ? 1 : 0);
		}
	}
	else if (command[0] != '@') hs_compile_and_evaluate(command);
	else platform_log("M0 world: unknown direct command %s", command);
}

static void m0_world_tick(void)
{
	static boolean metadata;
	static double start;
	static long last_sequence;
	static double capture_time;
	static short captures;
	static boolean voted_once;
	char const *path = getenv("M0_WORLD_PLAN");
	FILE *file;
	char line[1024], command[960];
	double seconds;
	long sequence, players = 0;
	struct data_iterator iterator;
	struct player_datum *player;
	if (!voted_once && getenv("M0_WORLD_SKIP_INTRO") && game_time_get() >= 90)
	{
		short votes, voters;
		boolean voted;
		if (network_coop_skip_vote_status(&votes, &voters, &voted))
		{
			voted_once = network_coop_vote_skip();
			m0_emit("world_vote", game_time_get(), votes, voters, voted_once, 0, 0);
		}
	}
	if (!metadata) { m0_world_metadata(); metadata = TRUE; }
	if (getenv("M0_WORLD_RENDER") && captures < 24 && coop_m0_clock() - capture_time >= 5)
	{
		capture_time = coop_m0_clock();
		captures++;
		platform_screenshot_request();
		m0_emit("world_screenshot", game_time_get(), captures, global_structure_bsp_index_get(), 0, 0, 0);
	}
	if (game_time_get() % 3 == 0) m0_world_capture();
	if (!coop_host() || !path) return;
	if (!start)
	{
		if (cinematic_in_progress()) return;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL)
			if (player->unit_index != NONE) players++;
		/* A device-only probe may run with one live unit; record that limitation. */
		if (players < (getenv("M0_WORLD_ONE_UNIT") ? 1 : 2)) return;
		start = coop_m0_clock();
		m0_emit("world_plan_start", game_time_get(), global_structure_bsp_index_get(), players, 0, 0, 0);
	}
	/* A trusted local fixture schedule, with wall time surviving a revert.
	 * No production command parser, script or network message is changed. */
	if (game_time_get() % 3 != 0) return;
	file = fopen(path, "r");
	if (!file) return;
	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, "%ld|%lf|%959[^\r\n]", &sequence, &seconds, command) != 3 ||
			sequence <= last_sequence || seconds < 0 || coop_m0_clock() - start < seconds) continue;
		last_sequence = sequence;
		m0_emit("world_command", game_time_get(), sequence, seconds, global_structure_bsp_index_get(), 0, 0);
		platform_log("M0 world command %ld: %s", sequence, command);
		m0_world_command(command);
		m0_recovery_capture();
	}
	fclose(file);
}
