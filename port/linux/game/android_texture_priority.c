/* Recent HUD pixels get first use of the normal CE cache each frame.
No extra pixel arena, changed bitmap formats or relaxed live-resource locks.
This comparison learns actual HUD requests, including custom-named bitmaps;
new HUD elements may still miss on their first frame. */
#ifdef HALO_ANDROID
#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "cache/texture_cache.h"
#include "memory/lruv_cache.h"
#include "rasterizer/rasterizer_swizzle.h"
#include "custom_edition_cache.h"
#include "mcc_cache.h"
#include "android_texture_priority.h"

#define ANDROID_HUD_BITMAPS 128
#define ANDROID_HUD_BYTES (4UL * 1024UL * 1024UL)
#define ANDROID_HUD_CACHED_FLAG (1 << 7) /* bitmap_data's cached-pixels flag */
extern void android_texture_test_log(const char *format, ...);

struct android_hud_bitmap
{
	struct bitmap_data *bitmap;
	unsigned long bytes, seen;
};
static struct android_hud_bitmap hud_bitmaps[ANDROID_HUD_BITMAPS];
static unsigned long hud_frame, hud_bytes, hud_count, hud_next_report;
static unsigned long hud_requests, hud_missing, hud_preloads, hud_ready, hud_rejected;
static unsigned long recovery_attempts, recovery_deferred;
static int hud_scope, hud_preparing, recovery_used;

static int android_texture_priority_active(void)
{
	return custom_edition_cache_tags_loaded() && !mcc_cache_tags_loaded();
}

static void android_texture_priority_report(void)
{
	android_texture_test_log("hud_cache: frame=%lu tracked=%lu tracked_bytes=%lu limit_bytes=%lu "
		"requests=%lu missing=%lu preload_requests=%lu preload_ready=%lu rejected=%lu "
		"recovery_attempts=%lu recovery_deferred=%lu (cumulative; HUD budget is inside 36 MiB)",
		hud_frame, hud_count, hud_bytes, ANDROID_HUD_BYTES, hud_requests, hud_missing,
		hud_preloads, hud_ready, hud_rejected, recovery_attempts, recovery_deferred);
}

void android_texture_priority_reset(void)
{
	if (hud_requests || recovery_attempts || recovery_deferred)
		android_texture_priority_report();
	memset(hud_bitmaps, 0, sizeof(hud_bitmaps));
	hud_frame = hud_bytes = hud_count = hud_next_report = 0;
	hud_requests = hud_missing = hud_preloads = hud_ready = hud_rejected = 0;
	recovery_attempts = recovery_deferred = 0;
	hud_scope = hud_preparing = recovery_used = 0;
}

void android_texture_priority_forget(struct bitmap_data *bitmap)
{
	long index;
	for (index = 0; index < ANDROID_HUD_BITMAPS; index++)
		if (hud_bitmaps[index].bitmap == bitmap && bitmap)
		{
			hud_bytes -= hud_bitmaps[index].bytes;
			hud_count--;
			memset(&hud_bitmaps[index], 0, sizeof(hud_bitmaps[index]));
		}
}

void android_texture_priority_scope(int enabled)
{
	hud_scope = enabled && android_texture_priority_active();
}

int android_texture_priority_collecting(void)
{
	return hud_scope && !hud_preparing;
}

void android_texture_priority_note(struct bitmap_data *bitmap)
{
	long index, slot = NONE, bytes;
	if (!hud_scope || hud_preparing || !bitmap || !(bitmap->flags & ANDROID_HUD_CACHED_FLAG))
		return;
	hud_requests++;
	for (index = 0; index < ANDROID_HUD_BITMAPS; index++)
	{
		if (hud_bitmaps[index].bitmap == bitmap)
		{
			hud_bitmaps[index].seen = hud_frame;
			return;
		}
		if (!hud_bitmaps[index].bitmap && slot == NONE)
			slot = index;
	}
	bytes = MAX(rasterizer_xbox_bitmap_get_pixel_data_size(bitmap), bitmap->pixels_size);
	if (bytes <= 0 || bytes > ANDROID_HUD_BYTES || slot == NONE)
	{
		hud_rejected++;
		return;
	}
	/* Match LRU allocation granularity, including linear-row padding. */
	bytes = ((unsigned long)bytes + 0x3fffUL) & ~0x3fffUL;
	if ((unsigned long)bytes > ANDROID_HUD_BYTES - hud_bytes)
	{
		hud_rejected++;
		return;
	}
	hud_bitmaps[slot].bitmap = bitmap;
	hud_bitmaps[slot].bytes = (unsigned long)bytes;
	hud_bitmaps[slot].seen = hud_frame;
	hud_bytes += (unsigned long)bytes;
	hud_count++;
}

void android_texture_priority_result(struct bitmap_data *bitmap, int ready)
{
	if (hud_scope && !hud_preparing && bitmap && (bitmap->flags & ANDROID_HUD_CACHED_FLAG) && !ready)
		hud_missing++;
}

void android_texture_priority_begin(struct lruv_cache *cache)
{
	long index;
	unsigned char ready[ANDROID_HUD_BITMAPS];
	if (!android_texture_priority_active())
		return;
	hud_frame++;
	recovery_used = 0;
	hud_scope = 0;
	/* Forget inactive HUDs promptly after weapon, view or player changes. */
	for (index = 0; index < ANDROID_HUD_BITMAPS; index++)
		if (hud_bitmaps[index].bitmap && hud_frame - hud_bitmaps[index].seen > 2)
			android_texture_priority_forget(hud_bitmaps[index].bitmap);
	if (!cache || cache->page_size_bits != 14 ||
		cache->page_count != HALO_PORT_TEXTURE_CACHE_PAGE_COUNT)
		return; /* A stolen/reduced cache retains its original behavior. */
	hud_preparing = 1;
	memset(ready, 0, sizeof(ready));
	/* Touch all resident HUD entries before any missing entry can evict one. */
	for (index = 0; index < ANDROID_HUD_BITMAPS; index++)
		if (hud_bitmaps[index].bitmap)
			ready[index] = _texture_cache_bitmap_get_hardware_format(hud_bitmaps[index].bitmap, FALSE, FALSE) != NULL;
	for (index = 0; index < ANDROID_HUD_BITMAPS; index++)
		if (hud_bitmaps[index].bitmap)
		{
			hud_preloads++;
			if (ready[index] || _texture_cache_bitmap_get_hardware_format(hud_bitmaps[index].bitmap, FALSE, TRUE))
				hud_ready++;
		}
	hud_preparing = 0;
	if (hud_frame >= hud_next_report)
	{
		android_texture_priority_report();
		hud_next_report = hud_frame + 300;
	}
}

int android_texture_priority_recover(struct lruv_cache *cache)
{
	if (!android_texture_priority_active() || !cache ||
		cache->page_count != HALO_PORT_TEXTURE_CACHE_PAGE_COUNT)
		return 0;
	if (recovery_used)
	{
		recovery_deferred++;
		return 0;
	}
	recovery_used = 1;
	recovery_attempts++;
	return 1;
}
#endif
