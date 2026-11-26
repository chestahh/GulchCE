/* Test build only. Every real trace still runs. A direct-mapped table gives
   a lower bound on exact repeats across batches. Count final depth changes
   separately so moving model geometry never becomes a static-world result. */
#include <stdio.h>
enum { SURVEY_ENTRIES = 65536 };
static struct survey_ray
{
	unsigned int key[6], batch_id, leaves;
	int leaf_count, hit, has_final;
	float world_depth, final_depth;
} survey_table[SURVEY_ENTRIES];
static struct { struct survey_ray *entry; int repeated; float previous; } survey_pending[PLASMA_COLLISION_RAYS];
static unsigned int survey_batch_id;
static unsigned long long survey_rays, survey_repeats, survey_world_changes, survey_object_changes, survey_finals;
static struct collision_bsp const *survey_bsp;
static byte survey_breakables[MAXIMUM_BREAKABLE_SURFACES_PER_MAP / 8];

static void reuse_survey_begin(void)
{
	byte const *breakables = breakable_surface_flags_get();
	if (survey_bsp != batch.bsp || memcmp(survey_breakables, breakables, sizeof(survey_breakables)))
	{
		memset(survey_table, 0, sizeof(survey_table));
		memcpy(survey_breakables, breakables, sizeof(survey_breakables));
		survey_bsp = batch.bsp;
	}
	survey_batch_id++;
	memset(survey_pending, 0, sizeof(survey_pending));
}

static void reuse_survey_world(real_point3d const *point, real_vector3d const *vector, int hit,
	struct collision_bsp_test_vector_result const *result)
{
	unsigned int key[6], hash = 2166136261u, leaves = 2166136261u;
	float depth = hit ? result->t : 2.0f;
	int i, index = batch.ray - 1;
	struct survey_ray *entry;
	memcpy(key, point, sizeof(*point));
	memcpy(key + 3, vector, sizeof(*vector));
	for (i = 0; i < 6; i++)
	{
		hash ^= key[i];
		hash *= 16777619u;
		hash ^= hash >> 16;
	}
	for (i = 0; i < result->leaf_count; i++)
		leaves = (leaves ^ result->leaf_indices[i]) * 16777619u;
	entry = &survey_table[hash % SURVEY_ENTRIES];
	survey_rays++;
	if (entry->batch_id && entry->batch_id != survey_batch_id && !memcmp(key, entry->key, sizeof(key)))
	{
		survey_repeats++;
		if (entry->leaf_count != result->leaf_count || entry->leaves != leaves ||
			entry->world_depth != depth || entry->hit != hit) survey_world_changes++;
		else if (entry->has_final)
		{
			survey_pending[index].repeated = 1;
			survey_pending[index].previous = entry->final_depth;
		}
	}
	memcpy(entry->key, key, sizeof(key));
	entry->batch_id = survey_batch_id;
	entry->hit = hit;
	entry->world_depth = depth;
	entry->leaf_count = result->leaf_count;
	entry->leaves = leaves;
	entry->has_final = 0;
	survey_pending[index].entry = entry;
}

static void reuse_survey_end(float const *depths, int count)
{
	static FILE *log;
	int i;
	/* Reverse order ensures an evicted entry keeps the newest ray's depth. */
	for (i = count - 1; i >= 0; i--)
	{
		struct survey_ray *entry = survey_pending[i].entry;
		if (!entry) continue;
		if (survey_pending[i].repeated)
		{
			survey_finals++;
			if (depths[i] != survey_pending[i].previous) survey_object_changes++;
		}
		if (!entry->has_final) { entry->final_depth = depths[i]; entry->has_final = 1; }
	}
	if (survey_batch_id % 32 != 0) return;
	if (!log)
	{
		log = fopen("ray-reuse.csv", "w");
		if (log) fprintf(log, "batches,rays,repeats,world_changes,final_comparisons,object_depth_changes\n");
	}
	if (log)
	{
		fprintf(log, "%u,%llu,%llu,%llu,%llu,%llu\n", survey_batch_id, survey_rays, survey_repeats,
			survey_world_changes, survey_finals, survey_object_changes);
		fflush(log);
	}
}
