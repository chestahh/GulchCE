/* Automatic Android bitmap allocation diagnostics. */
#ifndef ANDROID_CACHE_TEST_H
#define ANDROID_CACHE_TEST_H
struct lruv_cache;
struct bitmap_data;
enum android_cache_test_event
{
	_android_cache_test_allocated,
	_android_cache_test_rejected,
	_android_cache_test_allocation_failed,
	_android_cache_test_fallback,
	_android_cache_test_event_count
};
void android_cache_test_reset(struct lruv_cache *cache, const char *reason);
void android_cache_test_tick(struct lruv_cache *cache);
void android_cache_test_record(struct lruv_cache *cache, struct bitmap_data const *bitmap,
	int event, long requested_bytes);
void android_cache_test_memory(unsigned long *free_bytes, unsigned long *largest_bytes);
#endif
