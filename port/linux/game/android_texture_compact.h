/* Android CE bitmap-cache compaction, called only after allocation failure. */
#ifndef ANDROID_TEXTURE_COMPACT_H
#define ANDROID_TEXTURE_COMPACT_H
struct lruv_cache;
struct android_texture_compact_result
{
	long moved_pages, moved_blocks, evicted_blocks;
	int status; /* 0: insufficient capacity, 1: compacted, 2: pending/busy */
};
typedef void (*android_texture_move_proc)(long block, long first_page, long page_count);
void android_texture_compact(struct lruv_cache *cache, long size,
	android_texture_move_proc move, struct android_texture_compact_result *result);
void android_texture_move_texels(void *destination, const void *source, unsigned long size);
#endif
