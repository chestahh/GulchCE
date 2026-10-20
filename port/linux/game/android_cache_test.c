/* Read the existing cache's page usage; never touch, evict or load a bitmap. */
#ifdef HALO_ANDROID
#include "cseries.h"
#include "cseries_windows.h"
#include "bitmaps/bitmap_group.h"
#include "memory/lruv_cache.h"
#include "tag_files/tag_files.h"
#include "custom_edition_cache.h"
#include "mcc_cache.h"
#include "android_cache_test.h"
#include "android_texture_compact.h"

void android_texture_test_log(const char *format, ...);
static unsigned long cache_events[_android_cache_test_event_count];
static unsigned long cache_next_summary, cache_next_detail[_android_cache_test_event_count];
static unsigned long cache_session;
static unsigned long compact_recovered, compact_pages, compact_evicted, compact_ms;
static unsigned long compact_busy, compact_capacity, compact_next_detail;

static void android_cache_test_snapshot(struct lruv_cache *cache, const char *reason)
{
	static byte usage[HALO_PORT_TEXTURE_CACHE_PAGE_COUNT];
	unsigned long occupied = 0, locked = 0, free_run = 0, largest_free = 0;
	unsigned long available_run = 0, largest_available = 0, physical_free, physical_largest;
	long page;

	if (!cache || cache->page_count < 0 || cache->page_count > HALO_PORT_TEXTURE_CACHE_PAGE_COUNT)
		return;
	lruv_cache_get_page_usage(cache, usage);
	for (page = 0; page < cache->page_count; page++)
	{
		occupied += (usage[page] & 1) != 0;
		locked += (usage[page] & (2 | 8)) != 0;
		free_run = usage[page] ? 0 : free_run + 1;
		available_run = (usage[page] & (2 | 8)) ? 0 : available_run + 1;
		if (free_run > largest_free) largest_free = free_run;
		if (available_run > largest_available) largest_available = available_run;
	}
	android_cache_test_memory(&physical_free, &physical_largest);
	android_texture_test_log("bitmap_cache: ms=%lu session=%lu reason=%s family=%s pages=%ld occupied=%lu "
		"locked=%lu largest_free_pages=%lu largest_evictable_span_pages=%lu page_bytes=%lu "
		"allocations=%lu rejected=%lu allocation_failed=%lu fallback=%lu physical_free=%lu physical_largest=%lu "
		"retries=%lu recovered=%lu moved_pages=%lu evicted=%lu compact_ms=%lu busy_skips=%lu capacity_skips=%lu",
		(unsigned long)system_milliseconds(), cache_session, reason,
		mcc_cache_tags_loaded() ? "mcc" : custom_edition_cache_tags_loaded() ? "ce" : "xbox/menu",
		cache->page_count, occupied, locked, largest_free, largest_available, 1UL << cache->page_size_bits,
		cache_events[0], cache_events[1], cache_events[2], cache_events[3], physical_free, physical_largest,
		cache_events[4], compact_recovered, compact_pages, compact_evicted, compact_ms, compact_busy, compact_capacity);
}

void android_cache_test_reset(struct lruv_cache *cache, const char *reason)
{
	android_cache_test_snapshot(cache, reason);
	memset(cache_events, 0, sizeof(cache_events));
	memset(cache_next_detail, 0, sizeof(cache_next_detail));
	cache_next_summary = 0;
	compact_recovered = compact_pages = compact_evicted = compact_ms = 0;
	compact_busy = compact_capacity = compact_next_detail = 0;
	cache_session++;
}

void android_cache_test_tick(struct lruv_cache *cache)
{
	unsigned long now = (unsigned long)system_milliseconds();

	if (now >= cache_next_summary)
	{
		android_cache_test_snapshot(cache, "periodic");
		cache_next_summary = now + 5000;
	}
}

void android_cache_test_record(struct lruv_cache *cache, struct bitmap_data const *bitmap,
	int event, long requested_bytes)
{
	static const char *names[] = { "allocated", "rejected", "allocation_failed", "fallback", "retry" };
	unsigned long now = (unsigned long)system_milliseconds();
	const char *name;

	if (event < 0 || event >= _android_cache_test_event_count)
		return;
	cache_events[event]++;
	if (event == _android_cache_test_retry || event == _android_cache_test_allocation_failed || event == _android_cache_test_fallback)
		android_texture_test_pressure(event == _android_cache_test_retry, event == _android_cache_test_allocation_failed,
			event == _android_cache_test_fallback, 0, 0);
	if (event == _android_cache_test_allocated || now < cache_next_detail[event] || !bitmap)
		return;
	cache_next_detail[event] = now + 1000;
	name = bitmap->tag_index != NONE ? tag_get_name(bitmap->tag_index) : NULL;
	android_texture_test_log("bitmap_cache_event: ms=%lu session=%lu event=%s requested=%ld bitmap=%s "
		"size=%dx%dx%d format=%d flags=%x mips=%d pixels_size=%ld block=%ld",
		now, cache_session, names[event], requested_bytes, name ? name : "(none)",
		bitmap->width, bitmap->height, bitmap->depth, bitmap->format, (unsigned int)bitmap->flags,
		bitmap->mipmap_count, bitmap->pixels_size, bitmap->cache_block_index);
	android_cache_test_snapshot(cache, names[event]);
}

void android_cache_test_compaction(const struct android_texture_compact_result *result,
	unsigned long elapsed, int recovered)
{
	unsigned long now = system_milliseconds();

	compact_recovered += recovered != 0;
	compact_pages += result->moved_pages;
	compact_evicted += result->evicted_blocks;
	compact_ms += elapsed;
	compact_busy += result->status == 2;
	compact_capacity += result->status == 0;
	android_texture_test_pressure(0, 0, 0, recovered != 0, elapsed);
	if (now < compact_next_detail)
		return;
	compact_next_detail = now + 1000;
	android_texture_test_log("bitmap_compact: ms=%lu session=%lu status=%d recovered=%d moved_pages=%ld "
		"moved_blocks=%ld evicted=%ld elapsed_ms=%lu", now, cache_session, result->status, recovered,
		result->moved_pages, result->moved_blocks, result->evicted_blocks, elapsed);
}
#endif
