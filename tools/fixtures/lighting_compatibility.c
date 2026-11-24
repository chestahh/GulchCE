/* Passive compatibility probe, built only by lighting_compatibility_build.py.
   Observe the policy actually received by each peer, not just its settings. */
#include "cseries.h"
#include "game/game.h"
#include "network_shield_effects.h"
#include "shield_glow.h"
#include "game/players.h"
#include "memory/data.h"
#include "custom_edition_cache.h"
#include "mcc_cache.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include <stdio.h>

int config_boolean(char const *name);
int lighting_compatibility_policy_received(void);

static int lighting_compatibility_players(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	int count = 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		if (!player->quit_out_of_game) count++;
	return count;
}

void lighting_compatibility_sample(void)
{
	static FILE *file;
	static long previous = -30;
	long tick = game_time_get();
	if (!game_in_progress() || (tick >= previous && tick - previous < 30)) return;
	previous = tick;
	if (!file)
	{
		file = fopen("lighting-policy.csv", "w");
		if (file) fprintf(file, "tick,connection,policy,shield_enabled,local_shield,local_plasma,local_needler,policy_received,active_players\n");
	}
	if (file)
	{
		fprintf(file, "%ld,%d,%d,%d,%d,%d,%d,%d,%d\n", tick, game_connection(),
			network_shield_effects(), shield_glow_enabled(), config_boolean("display.shield_glow"),
			config_boolean("display.plasma_lighting"), config_boolean("display.needler_lighting"),
			lighting_compatibility_policy_received(), lighting_compatibility_players());
		fflush(file);
	}
}

/* Sample after shadow preparation, including Off. Accumulate short flashes
   between one-second records so the probe does not miss their entire pulse. */
void modern_shadow_compatibility_sample(int mode, int maps, int sun, int spots)
{
	static FILE *file;
	static long previous = -30;
	static int seen, seen_sun, seen_spots;
	long tick = game_time_get();
	if (!game_in_progress() || game_time_menu_scene()) return;
	if (tick < previous) seen = seen_sun = seen_spots = 0;
	seen_sun |= sun;
	seen_spots |= spots;
	seen |= maps;
	if (tick >= previous && tick - previous < 30) return;
	previous = tick;
	if (!file) {
		file = fopen("shadow-state.csv", "w");
		if (file) fprintf(file, "tick,mode,maps,sun,spots,family,scenario,connection,players,policy,received,shield\n");
	}
	if (file) {
		fprintf(file, "%ld,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d,%d\n", tick, mode, seen, seen_sun, seen_spots,
            mcc_cache_tags_loaded() ? "MCC" : custom_edition_cache_tags_loaded() ? "CE" : "Xbox",
            global_scenario_index == NONE ? "" : tag_get_name(global_scenario_index), game_connection(),
            lighting_compatibility_players(), network_shield_effects(), lighting_compatibility_policy_received(),
            shield_glow_enabled());
		fflush(file);
	}
	seen = seen_sun = seen_spots = 0;
}
