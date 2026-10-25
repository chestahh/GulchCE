/* Passive Windows benchmark instrumentation, installed only by the test builder.
   Gameplay, light selection and host policy are never changed by this fixture. */
#include "cseries.h"
#include <xtl.h>
#include "game/game.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include "plasma_lighting.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *timings;
static double origin, previous, flushed, next_capture;
static long selected, lights, occlusion_updates;
static double occlusion_started, occlusion_seconds;
static int screenshot_frames;
static double clock_seconds(void)
{
	LARGE_INTEGER counter, frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (double)counter.QuadPart / (double)frequency.QuadPart;
}

void lighting_performance_lights(void)
{
	long i;
	selected = 0;
	lights = rasterizer_lights.light_count;
	for (i = 0; i < lights; i++)
		selected += plasma_lighting_selected(i) != 0;
}

void lighting_performance_occlusion(int updates)
{
	occlusion_updates = updates;
}

void lighting_performance_occlusion_begin(void)
{
	occlusion_started = clock_seconds();
}

void lighting_performance_occlusion_end(void)
{
	occlusion_seconds += clock_seconds() - occlusion_started;
}

int lighting_performance_capture(void)
{
	double elapsed;
	if (!origin) return FALSE;
	elapsed = clock_seconds() - origin;
	if (elapsed < next_capture) return FALSE;
	next_capture = elapsed + 60.0;
	/* Readback can delay this frame and drain the next frame's GPU queue. */
	screenshot_frames = 3;
	return TRUE;
}

void lighting_performance_frame(void)
{
	double now = clock_seconds();
	if (!origin)
	{
		origin = now;
		previous = now;
		next_capture = 35.0;
		timings = fopen("frames.csv", "w");
		if (!timings) return;
		setvbuf(timings, NULL, _IOFBF, 65536);
		fprintf(timings, "seconds,frame_ms,selected,scene_lights,occlusion_updates,capture_excluded,tick,occlusion_ms\n");
	}
	if (timings)
	{
		fprintf(timings, "%.6f,%.6f,%ld,%ld,%ld,%d,%ld,%.6f\n", now - origin,
			(now - previous) * 1000.0, selected, lights, occlusion_updates,
			screenshot_frames > 0, game_time_get(), occlusion_seconds * 1000.0);
		if (now - flushed >= 1.0)
		{
			fflush(timings);
			flushed = now;
		}
	}
	if (screenshot_frames) screenshot_frames--;
	previous = now;
	occlusion_seconds = 0;
}
