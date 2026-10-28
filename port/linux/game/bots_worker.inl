/* Optional path worker, inspired by thelinkin3000/halo-ce-universal's
BirchWoodGod-derived bots. This implementation owns three bounded jobs.
Only graph traversal runs off-thread; engine queries and path smoothing
stay on the game thread. Included by bots.c after bots_navigation.inl. */
#include "physics/breakable_surfaces.h"
#include <xtl.h>

void bots_path_find_snapshot(struct path_state *state);

struct bots_path_job
{
	void *request, *done;
	boolean pending, waypoint, navigation;
	long bot, unit, goal_surface, tick;
	short goal_kind, phase, link;
	real radius;
	real_point3d start, goal, original_goal, origin;
	struct structure_bsp *original_structure;
	struct structure_bsp structure;
	struct path_state state;
	byte *walkable;
	long capacity;
};
static struct
{
	boolean attempted;
	void *thread, *wake, *quit;
	struct bots_path_job *jobs;
} bots_worker;

static unsigned long __stdcall bots_worker_main(void *unused)
{
	long index;
	(void)unused;
	for (;;)
	{
		WaitForSingleObject(bots_worker.wake, INFINITE);
		if (WaitForSingleObject(bots_worker.quit, 0) == WAIT_OBJECT_0) break;
		for (index = 0; index < BOTS_SEARCHES_A_TICK; index++)
		{
			struct bots_path_job *job = &bots_worker.jobs[index];
			if (WaitForSingleObject(job->request, 0) != WAIT_OBJECT_0) continue;
			bots_path_find_snapshot(&job->state);
			SetEvent(job->done);
		}
	}
	return 0;
}

static void bots_worker_stop(void)
{
	long index;
	if (bots_worker.thread)
	{
		SetEvent(bots_worker.quit);
		SetEvent(bots_worker.wake);
		WaitForSingleObject(bots_worker.thread, INFINITE);
		CloseHandle(bots_worker.thread);
	}
	if (bots_worker.jobs)
	{
		for (index = 0; index < BOTS_SEARCHES_A_TICK; index++)
		{
			struct bots_path_job *job = &bots_worker.jobs[index];
			if (job->request) CloseHandle(job->request);
			if (job->done) CloseHandle(job->done);
			if (job->walkable) free(job->walkable);
		}
		free(bots_worker.jobs);
	}
	if (bots_worker.wake) CloseHandle(bots_worker.wake);
	if (bots_worker.quit) CloseHandle(bots_worker.quit);
	csmemset(&bots_worker, 0, sizeof(bots_worker));
}

static boolean bots_worker_start(void)
{
	long index;
	char const *fixed = getenv("HALO_FIXED_TICK");
	if (bots_worker.attempted) return bots_worker.thread != NULL;
	bots_worker.attempted = TRUE;
	if (!bots_worker_enabled || (fixed && atoi(fixed))) return FALSE;
	bots_worker.jobs = (struct bots_path_job *)calloc(BOTS_SEARCHES_A_TICK, sizeof(struct bots_path_job));
	bots_worker.wake = CreateEventA(NULL, FALSE, FALSE, NULL);
	bots_worker.quit = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!bots_worker.jobs || !bots_worker.wake || !bots_worker.quit) goto failed;
	for (index = 0; index < BOTS_SEARCHES_A_TICK; index++)
	{
		bots_worker.jobs[index].request = CreateEventA(NULL, FALSE, FALSE, NULL);
		bots_worker.jobs[index].done = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (!bots_worker.jobs[index].request || !bots_worker.jobs[index].done) goto failed;
	}
	bots_worker.thread = CreateThread(NULL, 256 * 1024, bots_worker_main, NULL, 0, NULL);
	if (!bots_worker.thread) goto failed;
	platform_log("bots: snapshot path worker enabled");
	return TRUE;
failed:
	bots_worker_stop();
	bots_worker.attempted = TRUE;
	platform_log("bots: path worker unavailable; using synchronous searches");
	return FALSE;
}

/* Events transfer ownership of each job. A completed stale answer may be
discarded, but an in-flight job is never overwritten, even after respawn. */
static struct bots_path_job *bots_worker_job(struct bots_bot *bot)
{
	long index;
	if (!bots_worker.thread) return NULL;
	for (index = 0; index < BOTS_SEARCHES_A_TICK; index++)
	{
		struct bots_path_job *job = &bots_worker.jobs[index];
		struct bots_bot *owner;
		struct bots_nav_route *route;
		if (!job->pending) continue;
		owner = &bots_globals.bots[job->bot];
		route = &bots_navigation.routes[job->bot];
		if (WaitForSingleObject(job->done, 0) == WAIT_OBJECT_0 &&
			(owner->unit_index != job->unit || owner->goal_kind != job->goal_kind ||
			bots_distance3d(&owner->goal_point, &job->original_goal) > 1.0f ||
			bots_distance3d(&owner->last_position, &job->origin) > 3.0f ||
			route->phase != job->phase || route->link != job->link || game_time_get() - job->tick > TICKS_PER_SECOND))
		{
			job->pending = FALSE;
			continue;
		}
		if (owner == bot) return job;
	}
	return NULL;
}

static boolean bots_worker_submit(struct bots_bot *bot, struct path_state *state,
	real_point3d const *start, real_point3d const *goal, long goal_surface,
	boolean waypoint, boolean navigation, real radius)
{
	struct bots_path_job *job = NULL;
	struct collision_bsp const *bsp;
	long index, count = state->structure->pathfinding_surfaces.count;
	long const *broken;
	if (!bots_worker_start() || count <= 0) return FALSE;
	for (index = 0; index < BOTS_SEARCHES_A_TICK; index++)
		if (!bots_worker.jobs[index].pending) { job = &bots_worker.jobs[index]; break; }
	if (!job) return FALSE;
	if (job->capacity < count)
	{
		byte *memory = (byte *)realloc(job->walkable, count);
		if (!memory) return FALSE;
		job->walkable = memory;
		job->capacity = count;
	}
	job->structure = *state->structure;
	job->original_structure = state->structure;
	csmemcpy(job->walkable, state->structure->pathfinding_surfaces.address, count);
	job->structure.pathfinding_surfaces.address = job->walkable;
	/* Snapshot mutable breakage before handing immutable BSP geometry to
	the worker. Map disposal joins it before that geometry can be freed. */
	bsp = TAG_BLOCK_GET_ELEMENT(&state->structure->collision_bsp, 0, struct collision_bsp);
	broken = (long const *)breakable_surface_flags_get();
	if (!state->input.ignore_broken_surfaces)
		for (index = 0; index < count && index < bsp->surfaces.count; index++)
		{
			struct collision_surface *surface = TAG_BLOCK_GET_ELEMENT(&bsp->surfaces, index, struct collision_surface);
			if (TEST_FLAG(job->walkable[index], 7) && TEST_FLAG(surface->flags, 3) && broken &&
				surface->breakable_surface_index < state->structure->breakable_surfaces.count &&
				!BIT_VECTOR_TEST_FLAG(broken, surface->breakable_surface_index))
				SET_FLAG(job->walkable[index], BOTS_PATHFINDING_WALKABLE_BIT, FALSE);
		}
	job->state = *state;
	job->state.structure = &job->structure;
	job->state.input.ignore_broken_surfaces = TRUE;
	job->bot = (long)(bot - bots_globals.bots);
	job->unit = bot->unit_index;
	job->goal_kind = bot->goal_kind;
	job->original_goal = bot->goal_point;
	job->origin = bot->last_position;
	job->phase = bots_navigation.routes[job->bot].phase;
	job->link = bots_navigation.routes[job->bot].link;
	job->tick = game_time_get();
	job->start = *start;
	job->goal = *goal;
	job->goal_surface = goal_surface;
	job->waypoint = waypoint;
	job->navigation = navigation;
	job->radius = radius;
	job->pending = TRUE;
	ResetEvent(job->done);
	SetEvent(job->request);
	SetEvent(bots_worker.wake);
	return TRUE;
}
