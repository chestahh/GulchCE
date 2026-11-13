/* Additional M0 fixtures in a generated copy only. Private maps are unchanged. */
#include "items/weapons.h"
#include "mcc_cache.h"
#include "mcc_campaign.h"
#include "mcc_game_speed.h"
#include "mcc_grenades.h"

static void m0_domains_capture(void)
{
	struct object_iterator iterator;
	struct object_datum *object;
	struct hud_timer_state timer;
	long tick = game_time_get();
	short index;

	hud_messaging_port_timer_get(&timer);
	m0_emit("timer_layout", tick, timer.x, timer.y, timer.corner, scripted_hud_get_timer_ticks(), 0);
	if (mcc_cache_tags_loaded())
		m0_emit("mcc_state", tick, mcc_game_speed_client(game_time_get_speed()),
			mcc_campaign_effects_gain(), mcc_campaign_mix_gain(TRUE), mcc_grenades_type_count(), 0);
	object_iterator_new(&iterator, _object_mask_weapon | _object_mask_unit, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		if (object->object.type == _object_type_weapon)
		{
			struct weapon_datum *weapon = weapon_get(iterator.index);
			for (index = 0; index < NUMBEROF(weapon->weapon.magazines); index++)
				m0_emit("magazine", tick, iterator.index, index, weapon->weapon.magazines[index].rounds_total,
					weapon->weapon.magazines[index].rounds_loaded, weapon->weapon.magazines[index].state);
		}
		if (TEST_FLAG(_object_mask_unit, object->object.type) && mcc_cache_tags_loaded())
			m0_emit("mcc_grenades", tick, iterator.index, mcc_grenades_get(iterator.index, 2),
				mcc_grenades_get(iterator.index, 3), 0, 0);
	}
}

static long m0_domains_create(long definition, long unit_index, short ordinal)
{
	struct object_placement_data placement;
	long object_index;

	object_placement_data_new(&placement, definition, NONE);
	placement.flags |= FLAG(_new_object_never_automatically_delete_bit);
	placement.position = object_get(unit_index)->object.position;
	placement.position.x += 6.0f + ordinal;
	object_index = object_new(&placement);
	m0_emit("domain_object", game_time_get(), ordinal, object_index, definition, 0, 0);
	return object_index;
}

static void m0_domains_clear_inventory(long unit_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	long weapons[MAXIMUM_WEAPONS_PER_UNIT];
	short index;

	/* Save the handles before deletion callbacks alter the inventory. */
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		weapons[index] = unit->unit.weapon_object_indices[index];
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		if (weapons[index] != NONE && object_try_and_get(weapons[index])) object_delete(weapons[index]);
}

static void m0_domains_fixture(void)
{
	static long start = NONE, unit_index = NONE, definition = NONE;
	static long fixtures[5] = {NONE, NONE, NONE, NONE, NONE};
	static short phase;
	struct data_iterator iterator;
	struct player_datum *player;
	struct unit_datum *unit;
	struct object_datum *weapon;
	unsigned long allies[GAME_ALLEGIANCE_BITVECTOR_LONGS], friends[GAME_ALLEGIANCE_BITVECTOR_LONGS];
	short rounds[2], index;
	long tick = game_time_get(), count = 0;
	boolean empty = !strcmp(getenv("M0_RECOVERY_DOMAINS"), "empty");
	boolean changed = phase == 2;

	if (!coop_host()) return;
	if (start == NONE)
	{
		if (cinematic_in_progress()) return;
		data_iterator_new(&iterator, player_data);
		while ((player = data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index == NONE) continue;
			count++;
			unit_index = player->unit_index;
		}
		if (count < 2) return;
		unit = unit_get(unit_index);
		for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
		{
			weapon = object_try_and_get(unit->unit.weapon_object_indices[index]);
			if (weapon && weapon->object.type == _object_type_weapon) { definition = weapon->definition_index; break; }
		}
		if (definition == NONE) return;
		start = tick;
		m0_emit("domain_context", tick, unit_index, definition, empty,
			mcc_cache_tags_loaded() ? mcc_grenades_type_count() : 0, 0);
	}
	if (phase >= 7 || tick - start < (phase < 4 ? phase * 90 : (phase + 1) * 90)) return;
	m0_emit("fixture_phase", tick, phase, unit_index, 0, NONE, 0);
	if (phase == 0)
	{
		player_input_enable(FALSE);
		for (index = 0; index < 4; index++)
		{
			fixtures[index] = m0_domains_create(definition, unit_index, index);
			if (fixtures[index] == NONE) { phase = 7; return; }
		}
		objects_scripting_attach(fixtures[1], "", fixtures[2], "");
		if (empty) m0_domains_clear_inventory(unit_index);
	}
	if (phase == 2)
	{
		object_delete(fixtures[0]);
		objects_scripting_detach(fixtures[1], fixtures[2]);
		fixtures[4] = m0_domains_create(definition, unit_index, 4);
		if (empty && fixtures[4] != NONE)
			m0_emit("inventory_add", tick, unit_index, fixtures[4],
				unit_add_weapon_to_inventory(unit_index, fixtures[4], TRUE), 0, 0);
		else if (!empty) m0_domains_clear_inventory(unit_index);
	}
	if (phase == 0 || phase == 2)
	{
		objects_scripting_set_scale(fixtures[3], changed ? 2.0f : 0.5f, 0);
		rounds[0] = changed ? 4 : 20;
		rounds[1] = 0;
		weapon_set_total_rounds(fixtures[3], rounds);
		scripted_hud_set_timer_time(changed ? 3 : 2, 0);
		scripted_hud_show_timer(TRUE);
		scripted_hud_pause_timer(TRUE);
		scripted_hud_set_timer_position(changed ? 100 : 20, changed ? 80 : 30, 0);
		scripted_hud_set_timer_warning_cutoff(0, changed ? 20 : 10);
		game_allegiance_get_teams(allies, friends);
		/* Two unused team relationships: exercise saved vectors without provoking nearby actors. */
		allies[3] = changed ? allies[3] & ~1UL : allies[3] | 1UL;
		friends[3] = changed ? friends[3] & ~1UL : friends[3] | 1UL;
		game_allegiance_set_teams(allies, friends);
		if (mcc_cache_tags_loaded())
		{
			mcc_game_speed_set(changed ? 0.5f : 0.75f);
			hs_compile_and_evaluate(changed ? "(sound_set_effects_gain 0.75)" : "(sound_set_effects_gain 0.25)");
			hs_compile_and_evaluate(changed ? "(sound_set_music_gain 0.875)" : "(sound_set_music_gain 0.5)");
			for (index = 2; index < mcc_grenades_type_count(); index++)
				mcc_grenades_set(unit_index, index, changed ? 3 : 1);
		}
	}
	if (phase == 1) hs_compile_and_evaluate("(game_save_totally_unsafe)");
	if (phase == 3 || phase == 5) hs_compile_and_evaluate("(game_revert)");
	phase++;
	m0_recovery_capture();
}
