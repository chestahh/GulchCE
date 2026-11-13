/* M0 diagnostic only, included in a generated network_coop.c copy. */
void m0_recovery_hud(void);
void m0_recovery_nav(void);
void m0_recovery_objective(short ordinal);
void m0_recovery_epoch(void);

static void m0_recovery_capture(void)
{
	struct object_iterator objects;
	struct object_datum *object;
	struct data_iterator players;
	struct player_datum *player;
	struct hud_timer_state timer;
	unsigned long allies[GAME_ALLEGIANCE_BITVECTOR_LONGS], friends[GAME_ALLEGIANCE_BITVECTOR_LONGS];
	long tick = game_time_get();
	short index;

	if (!network_coop_active()) return;
	m0_emit("state_begin", tick, global_structure_bsp_index_get(), coop_host(), 0, 0, 0);
	m0_emit("hud_flags", tick, hud_scripted_globals->show_hud, hud_scripted_globals->show_hud_help_text,
		hud_unit_port_script_flags(), hud_weapon_port_script_flags(), player_input_enabled());
	m0_emit("presentation", tick, cinematic_in_progress(), cinematic_globals->show_letterbox,
		*director_camera_scripted, player_effect_port_scripted_active(), objects_get_activating_cluster_index());
	hud_messaging_port_timer_get(&timer);
	m0_emit("timer", tick, timer.enabled, timer.paused, timer.ticks, timer.reference_time, timer.flash_cutoff);
	game_allegiance_get_teams(allies, friends);
	for (index = 0; index < GAME_ALLEGIANCE_BITVECTOR_LONGS; index++)
		m0_emit("allegiance", tick, index, allies[index], friends[index], 0, 0);
	m0_emit("vitality_cache", tick, players_vitality.set, players_vitality.maximum_body,
		players_vitality.maximum_shield, 0, 0);
	m0_emit("hud_cache", tick, host_hud_state.objective, host_hud_state.nav_point_count,
		host_attachment_count, host_looping_sound_count, 0);
	for (index = 0; index < host_hud_state.nav_point_count; index++)
		m0_emit("nav_cache", tick, host_hud_state.nav_points[index].kind,
			host_hud_state.nav_points[index].target, host_hud_state.nav_points[index].marker,
			host_hud_state.nav_points[index].nav_index, host_hud_state.nav_points[index].vertical_offset);
	for (index = 0; index < host_attachment_count; index++)
		m0_emit("attachment_cache", tick, host_attachments[index].parent_index, host_attachments[index].child_index,
			host_attachments[index].parent_marker_index, host_attachments[index].child_marker_index, 0);
	for (index = 0; index < host_looping_sound_count; index++)
		m0_emit("loop_cache", tick, host_looping_sounds[index].definition_index, host_looping_sounds[index].object_index,
			host_looping_sounds[index].scale, host_looping_sounds[index].start_time, host_looping_sounds[index].length);
	for (index = 0; index < MAXIMUM_DEVICE_GROUPS; index++)
	{
		real value;
		word flags;
		boolean runtime;
		if (device_group_network_get(index, &value, &flags, &runtime))
			m0_emit("device_group", tick, index, value, flags, runtime, 0);
	}
	data_iterator_new(&players, player_data);
	while ((player = data_iterator_next(&players)) != NULL)
		m0_emit("roster", tick, players.datum_index, player->unit_index, player->team_index,
			player->network_player_data.machine_index, player->network_player_data.controller_index);
	object_iterator_new(&objects, _object_mask_all, 0);
	while ((object = object_iterator_next(&objects)) != NULL)
	{
		m0_emit("object", tick, objects.index, object->definition_index, object->object.type,
			object->object.name_index, object->object.parent_object_index);
		m0_emit("object_vitality", tick, objects.index, object->object.maximum_body_vitality,
			object->object.maximum_shield_vitality, object->object.body_vitality, object->object.shield_vitality);
		m0_emit("object_pose", tick, objects.index, object->object.position.x, object->object.position.y,
			object->object.position.z, object->object.scale);
		if (TEST_FLAG(_object_mask_unit, object->object.type))
		{
			struct unit_datum *unit = unit_get(objects.index);
			for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
				m0_emit("inventory", tick, objects.index, index, unit->unit.weapon_object_indices[index], 0, 0);
			m0_emit("unit", tick, objects.index, unit->unit.parent_seat_index,
				unit->unit.grenade_counts[0], unit->unit.grenade_counts[1], unit->unit.player_index);
		}
		if (TEST_FLAG(_object_mask_device, object->object.type))
		{
			struct device_datum *device = device_get(objects.index);
			m0_emit("device", tick, objects.index, device->device.power, device->device.position,
				device->device.power_group_index, device->device.position_group_index);
		}
	}
	m0_recovery_hud();
	m0_recovery_nav();
	m0_recovery_epoch();
	m0_emit("state_end", tick, 0, 0, 0, 0, 0);
}

static void m0_recovery_fixture(void)
{
	static long start = NONE, unit_index = NONE;
	static short phase, team = NONE, group = NONE;
	struct data_iterator iterator;
	struct player_datum *player;
	long tick = game_time_get(), count = 0;
	real value;
	word flags;
	boolean runtime;
	short index;

	if (!getenv("M0_RECOVERY") || !coop_host()) return;
	if (start == NONE)
	{
		if (cinematic_in_progress()) return;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index == NONE) continue;
			count++;
			unit_index = player->unit_index;
			team = (short)player->team_index;
		}
		if (count < 2) return;
		start = tick;
		for (index = 0; index < MAXIMUM_DEVICE_GROUPS; index++)
			if (device_group_network_get(index, &value, &flags, &runtime) && !runtime)
			{
				group = index;
				break;
			}
	}
	if (phase >= 7 || tick - start < (phase < 4 ? phase * 90 : (phase + 1) * 90)) return;
	m0_emit("fixture_phase", tick, phase, unit_index, team, group, 0);
	if (phase == 0 || phase == 2)
	{
		boolean changed = phase == 2;
		scripted_show_hud(changed);
		scripted_show_hud_help_text(changed);
		player_input_enable(FALSE);
		m0_recovery_objective(changed ? 1 : 0);
		network_coop_set_players_vitality(unit_index, TRUE, changed ? 2.0f : 1.0f, changed ? 3.0f : 1.0f);
		if (hud_globals && hud_globals->waypoint.arrows.count > 0)
		{
			if (changed) hud_deactivate_team_nav_point_with_object(team, unit_index);
			else hud_activate_team_nav_point_with_object(0, team, unit_index, 0.5f);
		}
		if (group != NONE && device_group_network_get(group, &value, &flags, &runtime))
			device_group_network_set(group, changed ? 0.75f : 0.25f, flags, TRUE);
	}
	if (phase == 1) hs_compile_and_evaluate("(game_save_totally_unsafe)");
	if (phase == 3 || phase == 5) hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
