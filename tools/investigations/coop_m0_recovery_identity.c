/* M0 only: normal allocation, deletion and inventory interfaces. No pool edits. */
static long m0_identity_create(long definition, long unit_index, short stage, short attempt)
{
	struct object_placement_data placement;
	long object_index;

	object_placement_data_new(&placement, definition, NONE);
	placement.flags |= FLAG(_new_object_never_automatically_delete_bit);
	placement.position = object_get(unit_index)->object.position;
	placement.position.x += 6.0f;
	object_index = object_new(&placement);
	m0_emit("identity_new", game_time_get(), stage, object_index, definition, attempt, 0);
	return object_index;
}

static void m0_identity_clear(long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	long weapons[MAXIMUM_WEAPONS_PER_UNIT];
	short index;

	/* Deletion callbacks change the inventory; keep the original handles. */
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		weapons[index] = unit->unit.weapon_object_indices[index];
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		if (weapons[index] != NONE && object_try_and_get(weapons[index]))
			object_delete_immediately(weapons[index]);
}

static void m0_domains_fixture(void)
{
	static long start = NONE, saved_weapon = NONE, replacement = NONE;
	static long units[2], definitions[2] = {NONE, NONE};
	static short phase;
	struct data_iterator iterator;
	struct player_datum *player;
	struct unit_datum *unit;
	struct object_datum *object;
	short count = 0, index, attempt, rounds[2];
	long tick = game_time_get();

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
				if (!object || object->object.type != _object_type_weapon) continue;
				if (definitions[0] == NONE) definitions[0] = object->definition_index;
				else if (object->definition_index != definitions[0]) definitions[1] = object->definition_index;
			}
		}
		if (count != 2 || definitions[1] == NONE) return;
		start = tick;
		m0_emit("identity_context", tick, units[0], units[1], definitions[0], definitions[1], 0);
	}
	if (phase >= 7 || tick - start < (phase < 4 ? phase * 90 : (phase + 1) * 90)) return;
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
	}
	if (phase == 2)
	{
		object_delete_immediately(saved_weapon);
		/* Keep intervening allocations alive. Never force an index or alter the
		 * allocator: classify an unreached target as an incomplete fixture. */
		for (attempt = 0; attempt < 64; attempt++)
		{
			replacement = m0_identity_create(definitions[1], units[1], 1, attempt);
			if (replacement == NONE || (unsigned short)replacement == (unsigned short)saved_weapon) break;
		}
		if (replacement == NONE || (unsigned short)replacement != (unsigned short)saved_weapon)
		{
			m0_emit("identity_unavailable", tick, saved_weapon, replacement, attempt, 0, 0);
			phase = 7;
			return;
		}
		rounds[0] = 4;
		rounds[1] = 0;
		weapon_set_total_rounds(replacement, rounds);
		m0_emit("identity_add", tick, 1, units[1], replacement,
			unit_add_weapon_to_inventory(units[1], replacement, TRUE), 0);
	}
	if (phase == 1) hs_compile_and_evaluate("(game_save_totally_unsafe)");
	if (phase == 3 || phase == 5) hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
