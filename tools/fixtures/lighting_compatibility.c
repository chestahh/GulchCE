/* Passive compatibility probe, built only by lighting_compatibility_build.py.
   Observe the policy actually received by each peer, not just its settings. */
#include "cseries.h"
#include "game/game.h"
#include "network_shield_effects.h"
#include "shield_glow.h"
#include <stdio.h>
#include <string.h>

int config_boolean(char const *name);

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
		if (file) fprintf(file, "tick,connection,policy,shield_enabled,local_shield,local_plasma,local_needler\n");
	}
	if (file)
	{
		fprintf(file, "%ld,%d,%d,%d,%d,%d,%d\n", tick, game_connection(),
			network_shield_effects(), shield_glow_enabled(), config_boolean("display.shield_glow"),
			config_boolean("display.plasma_lighting"), config_boolean("display.needler_lighting"));
		fflush(file);
	}
}

/* Sample after shadow preparation, including Off. Accumulate short flashes
   between one-second records so the probe does not miss their entire pulse. */
const char *config_string(const char *name);
void modern_shadow_compatibility_sample(int maps)
{
	static FILE *file;
	static long previous = -30;
	static int seen;
	long tick = game_time_get();
	const char *mode = config_string("display.modern_shadows");
	if (!game_in_progress()) return;
	seen |= maps;
	if (tick >= previous && tick - previous < 30) return;
	previous = tick;
	if (!file) {
		file = fopen("shadow-state.csv", "w");
		if (file) fprintf(file, "tick,mode,maps\n");
	}
	if (file) {
		fprintf(file, "%ld,%d,%d\n", tick, !strcmp(mode,"hard") ? 1 : !strcmp(mode,"soft") ? 2 : 0, seen);
		fflush(file);
	}
	seen = 0;
}
