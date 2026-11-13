/* M0 only: use normal engine interfaces in generated diagnostic copies. */
static void m0_domains_fixture(void)
{
	static long start = NONE;
	static long weapons[2] = {NONE, NONE};
	static short phase;
	struct data_iterator iterator;
	struct player_datum *player;
	struct unit_datum *unit;
	struct object_datum *object;
	short index, count = 0, rounds[2];
	long tick = game_time_get();

	if (!coop_host()) return;
	if (start == NONE)
	{
		if (cinematic_in_progress()) return;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL && count < 2)
		{
			if (player->unit_index == NONE) continue;
			unit = unit_get(player->unit_index);
			weapons[count] = NONE;
			for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
			{
				object = object_try_and_get(unit->unit.weapon_object_indices[index]);
				if (object && object->object.type == _object_type_weapon)
				{
					weapons[count] = unit->unit.weapon_object_indices[index];
					break;
				}
			}
			if (weapons[count] == NONE) return;
			count++;
		}
		if (count != 2) return;
		start = tick;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index == NONE) continue;
			unit = unit_get(player->unit_index);
			for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
				if (unit->unit.weapon_object_indices[index] == weapons[0] || unit->unit.weapon_object_indices[index] == weapons[1])
					m0_emit("timing_weapon", tick, player->unit_index, unit->unit.weapon_object_indices[index], index, iterator.datum_index, 0);
		}
	}
	if (phase >= 7 || tick - start < (phase < 4 ? phase * 90 : (phase + 1) * 90)) return;
	m0_emit("fixture_phase", tick, phase, 0, 0, 0, 0);
	if (phase == 0 || phase == 2)
	{
		player_input_enable(FALSE);
		rounds[0] = phase == 0 ? 20 : 4;
		rounds[1] = 0;
		for (index = 0; index < 2; index++)
			weapon_set_total_rounds(weapons[index], rounds);
		scripted_hud_set_timer_time(0, phase == 0 ? 20 : 50);
		scripted_hud_show_timer(TRUE);
		scripted_hud_set_timer_warning_cutoff(0, 5);
	}
	if (phase == 1) hs_compile_and_evaluate("(game_save_totally_unsafe)");
	if (phase == 3 || phase == 5) hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
