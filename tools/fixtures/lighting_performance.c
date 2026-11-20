/* Windows benchmark instrumentation, installed only by the test builder.
   Ordinary runs are passive. HALO_LIGHTING_BENCH_SHIELD explicitly adds one
   stationary shield to an isolated test copy; host policy remains unchanged. */
#include "cseries.h"
#include <xtl.h>
#include "game/game.h"
#include "game/cheats.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include "plasma_lighting.h"
#include "modern_shadows.h"
#include "camera/observer.h"
#include "tag_files/tag_files.h"
#include "physics/collisions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *timings;
static double origin, previous, flushed, next_capture;
static long selected, lights, occlusion_updates, shadowed;
static double occlusion_started, occlusion_seconds;
static int screenshot_frames;
static FILE *profile;
enum { LIGHTING_PROFILE_PHASES = 20 };
static double phase_start[LIGHTING_PROFILE_PHASES], phase_time[LIGHTING_PROFILE_PHASES];
static long phase_calls[LIGHTING_PROFILE_PHASES];

/* Explicit isolated shield workload; ordinary performance runs stay passive. */
static void stationary_shield_scene(double elapsed)
{
	static int created;
	struct object_placement_data placement;
	struct observer_result const *camera;
	struct collision_result ground;
	real_vector3d down = {0, 0, -5};
	real_point3d from;
	long tag, object;
	if (!getenv("HALO_LIGHTING_BENCH_SHIELD") || created || elapsed < 2.0) return;
	created = TRUE;
	tag = tag_loaded('scen', "scenery\\c_field_generator\\c_field_generator");
	if (tag == NONE) { fprintf(stderr, "BENCH shield tag missing\n"); return; }
	camera = observer_get_camera(0);
	object_placement_data_new(&placement, tag, NONE);
	placement.position = camera->position;
	placement.position.x += camera->forward.i * 2.5f;
	placement.position.y += camera->forward.j * 2.5f;
	from = placement.position;
	from.z += 0.5f;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&from, &down, NONE, &ground)) placement.position.z = ground.point.z + 0.03f;
	placement.forward.i = -camera->forward.j;
	placement.forward.j = camera->forward.i;
	placement.forward.k = 0;
	normalize3d(&placement.forward);
	observer_up_from_forward(&placement.forward, &placement.up);
	object = object_new(&placement);
	fprintf(stderr, "BENCH shield created %ld\n", object);
}
static double clock_seconds(void)
{
	LARGE_INTEGER counter, frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return (double)counter.QuadPart / (double)frequency.QuadPart;
}

double lighting_performance_seconds(void)
{
	return origin ? clock_seconds() - origin : -1;
}

/* Explicit test route from the archived c40 exterior-door checkpoint. This
   operates the real door control and walks around the exterior buttress. */
static int lighting_snowy_route(XINPUT_GAMEPAD *pad, double elapsed)
{
    struct route_step { double seconds; short lx, ly, rx; int use; };
    static const struct route_step defaults[] = {
		{2,0,0,0,0}, {2.85,0,32767,0,0}, {.3,0,0,0,0},
		{1.1,32767,0,0,0}, {.3,0,0,0,0}, {.5,0,0,32767,0}, {.3,0,0,0,0},
		{.2,0,0,0,1}, {1,0,0,0,0}, {.95,0,-32767,0,0}, {.3,0,0,0,0},
		{.5,0,0,-32767,0}, {.3,0,0,0,0}, {5,0,32767,0,0}, {.3,0,0,0,0},
		{.5,0,0,32767,0}, {.3,0,0,0,0}, {3.5,0,32767,0,0}, {.3,0,0,0,0},
		{.5,0,0,-32767,0}
	};
    static struct route_step custom[128];
    static int ready, custom_count;
    const struct route_step *steps = defaults;
    int i, count = NUMBEROF(defaults);
    if (!ready)
    {
        const char *path = getenv("HALO_LIGHTING_BENCH_ROUTE_FILE");
        ready = 1;
        if (path)
        {
            FILE *file = fopen(path, "r");
            double seconds;
            int lx, ly, rx, use;
            if (file)
            {
                while (custom_count < NUMBEROF(custom) &&
                    fscanf(file, "%lf %d %d %d %d", &seconds, &lx, &ly, &rx, &use) == 5)
                {
                    if (!(seconds > 0 && seconds <= 30) || lx < -32767 || lx > 32767 ||
                        ly < -32767 || ly > 32767 || rx < -32767 || rx > 32767 || use < 0 || use > 7)
                        break;
                    custom[custom_count].seconds = seconds;
                    custom[custom_count].lx = (short)lx; custom[custom_count].ly = (short)ly;
                    custom[custom_count].rx = (short)rx; custom[custom_count].use = use;
                    custom_count++;
                }
                fclose(file);
            }
            fprintf(stderr, "BENCH custom route steps %d\n", custom_count);
        }
    }
    if (custom_count) { steps = custom; count = custom_count; }
	if (elapsed < 0) return FALSE;
	/* The benchmark owns its complete input, including after the route. */
	memset(pad, 0, sizeof(*pad));
    for (i = 0; i < count; i++)
	{
		if (elapsed < steps[i].seconds)
		{
			pad->sThumbLX = steps[i].lx; pad->sThumbLY = steps[i].ly; pad->sThumbRX = steps[i].rx;
			/* Route actions: use, ordinary grenade throw, grenade selection. */
			pad->bAnalogButtons[XINPUT_GAMEPAD_X] = (steps[i].use & 1) ? 255 : 0;
			pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] = (steps[i].use & 2) ? 255 : 0;
			pad->bAnalogButtons[XINPUT_GAMEPAD_Y] = (steps[i].use & 4) ? 255 : 0;
			break;
		}
		elapsed -= steps[i].seconds;
	}
    if (i == count && getenv("HALO_LIGHTING_BENCH_TURN"))
	{
		double turn = atof(getenv("HALO_LIGHTING_BENCH_TURN"));
		if (elapsed < (turn < 0 ? -turn : turn))
			pad->sThumbRX = turn < 0 ? -32767 : 32767;
	}
	return TRUE;
}

/* Route inspection uses ordinary controller movement in a copied checkpoint.
   It neither teleports the player nor changes the mission's actors/scripts. */
int lighting_performance_input(XINPUT_GAMEPAD *pad)
{
	const char *advance = getenv("HALO_LIGHTING_BENCH_ADVANCE");
	double elapsed;
	if (getenv("HALO_LIGHTING_BENCH_SNOWY_ROUTE"))
		return lighting_snowy_route(pad, lighting_performance_seconds());
	if (!advance) return FALSE;
	elapsed = lighting_performance_seconds();
	if (elapsed < 2) return FALSE;
	if (elapsed < 2 + atof(advance)) pad->sThumbLY = 32767;
	else if (getenv("HALO_LIGHTING_BENCH_TURN"))
	{
		double turn = atof(getenv("HALO_LIGHTING_BENCH_TURN"));
		if (elapsed < 2 + atof(advance) + (turn < 0 ? -turn : turn))
			pad->sThumbRX = turn < 0 ? -32767 : 32767;
	}
	return TRUE;
}

void lighting_profile_begin(int phase)
{
	phase_start[phase] = clock_seconds();
}

void lighting_profile_end(int phase)
{
	phase_time[phase] += clock_seconds() - phase_start[phase];
	phase_calls[phase]++;
}

void lighting_performance_lights(void)
{
	long i;
	selected = 0;
	shadowed = 0;
	lights = rasterizer_lights.light_count;
	for (i = 0; i < lights; i++)
	{
		selected += plasma_lighting_selected(i) != 0;
		shadowed += modern_shadows_selected(i) != 0;
	}
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
    if (getenv("HALO_LIGHTING_BENCH_ROUTE_CAPTURES"))
    {
        const struct observer_result *camera = observer_get_camera(0);
        fprintf(stderr, "BENCH route %.3f position %.6f %.6f %.6f forward %.6f %.6f %.6f\n",
            elapsed, camera->position.x, camera->position.y, camera->position.z,
            camera->forward.i, camera->forward.j, camera->forward.k);
    }
	next_capture = elapsed + (getenv("HALO_LIGHTING_BENCH_ROUTE_CAPTURES") ? 2.0 : 60.0);
	/* Readback can delay this frame and drain the next frame's GPU queue. */
	screenshot_frames = 3;
	return TRUE;
}

void lighting_performance_frame(void)
{
	int i;
	double now = clock_seconds();
	if (origin) stationary_shield_scene(now - origin);
	if (!origin)
	{
		/* Explicit controlled stress run, never part of the production build.
		   Keep the copied encounter active instead of measuring death/reload. */
		if (getenv("HALO_LIGHTING_BENCH_DEATHLESS"))
		{
			cheat.deathless_player = TRUE;
			fprintf(stderr, "BENCH controlled invulnerable player enabled\n");
		}
		origin = now;
		previous = now;
		next_capture = getenv("HALO_LIGHTING_BENCH_ROUTE_CAPTURES") ? 2.0 : 35.0;
		timings = fopen("frames.csv", "w");
		if (!timings) return;
		setvbuf(timings, NULL, _IOFBF, 65536);
		fprintf(timings, "seconds,frame_ms,selected,scene_lights,occlusion_updates,capture_excluded,tick,occlusion_ms,shadowed,sun_casters\n");
	}
	if (phase_calls[0] || phase_calls[1] || phase_calls[2] || phase_calls[3] || phase_calls[7])
	{
		if (!profile)
		{
			profile = fopen("profile.csv", "w");
			if (profile)
			{
				setvbuf(profile, NULL, _IOFBF, 65536);
				fprintf(profile, "seconds,trace_ms,field_ms,texture_ms,bind_ms,prepare_ms,light_scene_ms,upload_ms,render_ms,traces,fields,textures,binds,prepares,light_scenes,uploads,renders,model_field_ms,model_resolve_ms,collision_scene_ms,mask_upload_ms,shadow_ms,shadow_world_ms,shadow_objects_ms,shadow_draw_ms,shadow_state_ms,shadow_resources_ms,shadow_selection_ms,environment_models_ms\n");
			}
		}
		if (profile)
		{
			fprintf(profile, "%.6f", now-origin);
			for (i = 0; i < 8; i++) fprintf(profile, ",%.6f", phase_time[i]*1000);
			for (i = 0; i < 8; i++) fprintf(profile, ",%ld", phase_calls[i]);
			for (i = 8; i < LIGHTING_PROFILE_PHASES; i++) fprintf(profile, ",%.6f", phase_time[i]*1000);
			fputc('\n', profile);
		}
	}
	for (i = 0; i < LIGHTING_PROFILE_PHASES; i++) { phase_time[i] = 0; phase_calls[i] = 0; }
	if (timings)
	{
		fprintf(timings, "%.6f,%.6f,%ld,%ld,%ld,%d,%ld,%.6f,%ld,%d\n", now - origin,
			(now - previous) * 1000.0, selected, lights, occlusion_updates,
			screenshot_frames > 0, game_time_get(), occlusion_seconds * 1000.0, shadowed, modern_shadows_sun_casters());
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
