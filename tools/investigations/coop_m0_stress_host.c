/* Generated-only M0 fixture: two saved weapons, four recovery epochs. */
extern void m0_target_write(long target, long unit, long definition);
extern void m0_other_write(long target, long unit, long definition);
static void m0_domains_fixture(void)
{
	static long start = NONE, units[2], weapons[2], definition = NONE;
	static short phase;
	/* Keep the second/third restores over a minute apart: the existing
	 * third-revert guard must not turn the interruption into a respawn. */
	static long const schedule[] = {0, 90, 180, 270, 1620, 1710, 3510, 3600, 3870, 4170};
	struct data_iterator iterator;
	struct player_datum *player;
	struct object_datum *object;
	struct unit_datum *unit;
	long tick = game_time_get();
	short count = 0, index, slot, rounds[2];

	if (!coop_host()) return;
	/* Use the existing script-facing cancellation service only in the
	 * explicitly pinned fixture, after its real disk save has completed. */
	if (m0_stress_pin_active) main_save_cancel();
	if (start == NONE)
	{
		if (cinematic_in_progress()) return;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL && count < 2)
		{
			if (player->unit_index == NONE) continue;
			units[count++] = player->unit_index;
			unit = unit_get(player->unit_index);
			for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
			{
				object = object_try_and_get(unit->unit.weapon_object_indices[slot]);
				if (definition == NONE && object && object->object.type == _object_type_weapon)
					definition = object->definition_index;
			}
		}
		if (count != 2 || definition == NONE) return;
		start = tick;
	}
	if (phase >= NUMBEROF(schedule) || tick - start < schedule[phase]) return;
	m0_emit("stress_phase", tick, phase, 0, 0, 0, 0);
	if (phase == 0)
	{
		player_input_enable(FALSE);
		for (index = 0; index < 2; index++)
		{
			m0_identity_clear(units[index]);
			weapons[index] = m0_identity_create(definition, units[index], 0, index);
			if (weapons[index] == NONE) { phase = NUMBEROF(schedule); return; }
			rounds[0] = 20 + 4 * index;
			rounds[1] = 0;
			weapon_set_total_rounds(weapons[index], rounds);
			m0_emit("stress_target", tick, index, units[index], weapons[index], definition,
				unit_add_weapon_to_inventory(units[index], weapons[index], TRUE));
		}
		m0_target_write(weapons[0], units[0], definition);
		m0_other_write(weapons[1], units[1], definition);
	}
	if (phase == 1)
	{
		m0_stress_pin_requested = getenv("M0_PIN_CHECKPOINT") != NULL;
		hs_compile_and_evaluate("(game_save_totally_unsafe)");
	}
	if (phase == 2 || phase == 4 || phase == 6)
	{
		for (index = 0; index < 2; index++)
			if (object_try_and_get(weapons[index])) object_delete_immediately(weapons[index]);
	}
	if (phase == 3 || phase == 5 || phase == 7 || phase == 8)
		hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
