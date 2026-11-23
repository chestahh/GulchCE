/* Windows stock-renderer timing fixture, inserted only into an isolated build. */
#include "game/cheats.h"
#include "rasterizer/rasterizer_lights.h"
#include "units/units.h"
#include <stdio.h>
#include <stdlib.h>

static void stock_benchmark_frame(void)
{
	static FILE *frames;
	static double origin, previous, flushed;
	static int beam;
	LARGE_INTEGER count, frequency;
	double now;
	long unit;

	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&frequency);
	now = (double)count.QuadPart / (double)frequency.QuadPart;
	if (!origin)
	{
		origin = previous = now;
		beam = getenv("HALO_LIGHTING_BENCH_FLASHLIGHT") != NULL;
		if (getenv("HALO_LIGHTING_BENCH_DEATHLESS"))
		{
			cheat.deathless_player = TRUE;
			fprintf(stderr, "BENCH controlled invulnerable player enabled\n");
		}
		frames = fopen("frames.csv", "w");
		if (!frames)
			return;
		setvbuf(frames, NULL, _IOFBF, 65536);
		fputs("seconds,frame_ms,selected,scene_lights,occlusion_updates,capture_excluded,"
			"tick,occlusion_ms,shadowed,sun_casters,spot_mask\n", frames);
	}
	if (beam && now - origin > 1.0)
	{
		unit = player_control_get_unit_index(0);
		if (unit != NONE)
			unit_set_desired_flashlight_state(unit, TRUE);
	}
	if (frames)
	{
		fprintf(frames, "%.6f,%.6f,0,%d,0,0,%ld,0,0,0,0\n", now - origin,
			1000.0 * (now - previous), (int)rasterizer_lights.light_count, (long)game_time_get());
		if (now - flushed >= 1.0)
		{
			fflush(frames);
			flushed = now;
		}
	}
	previous = now;
}
