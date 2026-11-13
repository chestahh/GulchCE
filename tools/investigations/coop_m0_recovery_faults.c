/* M0 generated-only host fixture. Helpers come from the identity fixture. */
extern void m0_target_write(long target, long unit, long definition);
static void m0_domains_fixture(void)
{
	static long start = NONE, saved_weapon = NONE;
	static long units[2], definitions[2];
	static short phase;
	static long const schedule[7] = {0, 90, 180, 270, 990, 1080, 1440};
	struct data_iterator iterator;
	struct player_datum *player;
	struct unit_datum *unit;
	struct object_datum *object;
	short count = 0, index, attempt, rounds[2];
	long tick = game_time_get(), replacement = NONE;

	if (!coop_host()) return;
	if (start == NONE)
	{
		if (cinematic_in_progress()) return;
		definitions[0] = definitions[1] = NONE;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL && count < 2)
		{
			if (player->unit_index == NONE) continue;
			units[count++] = player->unit_index;
			unit = unit_get(player->unit_index);
			for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
			{
				object = object_try_and_get(unit->unit.weapon_object_indices[index]);
				if (definitions[0] == NONE && object && object->object.type == _object_type_weapon)
					definitions[0] = object->definition_index;
			}
		}
		if (count != 2 || definitions[0] == NONE) return;
		/* Bipeds belong to the ordinary replicated-object mask. Unnamed
		 * scenery does not, so it cannot establish client-side slot reuse. */
		object = object_get(units[1]);
		if (object->object.type != _object_type_biped) return;
		definitions[1] = object->definition_index;
		start = tick;
		m0_emit("identity_context", tick, units[0], units[1], definitions[0], definitions[1], 0);
	}
	if (phase >= 7 || tick - start < schedule[phase]) return;
	m0_emit("fixture_phase", tick, phase, 0, 0, 0, 0);
	if (phase == 0)
	{
		player_input_enable(FALSE);
		m0_identity_clear(units[0]);
		m0_identity_clear(units[1]);
		saved_weapon = m0_identity_create(definitions[0], units[0], 0, 0);
		if (saved_weapon == NONE) { phase = 7; return; }
		rounds[0] = 20;
		rounds[1] = 0;
		weapon_set_total_rounds(saved_weapon, rounds);
		m0_emit("identity_add", tick, 0, units[0], saved_weapon,
			unit_add_weapon_to_inventory(units[0], saved_weapon, TRUE), 0);
		/* Private runner metadata selects an injection target, never game state. */
		m0_target_write(saved_weapon, units[0], definitions[0]);
	}
	if (phase == 2 || phase == 4)
	{
		object_delete_immediately(saved_weapon);
		for (attempt = 0; attempt < 64; attempt++)
		{
			replacement = m0_identity_create(definitions[1], units[1], phase / 2, attempt);
			if (replacement == NONE || (unsigned short)replacement == (unsigned short)saved_weapon) break;
		}
		if (replacement == NONE || (unsigned short)replacement != (unsigned short)saved_weapon)
		{
			m0_emit("identity_unavailable", tick, saved_weapon, replacement, attempt, 0, 0);
			phase = 7;
			return;
		}
		m0_emit("cross_replacement", tick, phase / 2, saved_weapon, replacement,
			object_get(replacement)->object.type, definitions[1]);
	}
	if (phase == 1) hs_compile_and_evaluate("(game_save_totally_unsafe)");
	if (phase == 3 || phase == 5) hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
