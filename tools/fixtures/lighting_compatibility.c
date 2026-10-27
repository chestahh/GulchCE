/* Passive compatibility probe, built only by lighting_compatibility_build.py.
   Observe the policy actually received by each peer, not just its settings. */
#include "cseries.h"
#include "game/game.h"
#include "network_shield_effects.h"
#include "shield_glow.h"
#include <stdio.h>

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
