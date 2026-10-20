/* Keep current-frame entries alive while joining fragmented CE cache space.
The caller runs on the render thread, with CE's synchronous texture reads.
No pending read or busy resource may move, even if a future caller changes. */
#ifdef HALO_ANDROID
#include "cseries.h"
#include "memory/data.h"
#include "memory/lruv_cache.h"
#include "android_texture_compact.h"

void android_texture_compact(struct lruv_cache *cache, long size,
	android_texture_move_proc move, struct android_texture_compact_result *result)
{
	struct lruv_cache_block *block;
	long index, needed, available, free_pages, oldest, destination;
	unsigned long oldest_tick;

	memset(result, 0, sizeof(*result));
	if (!cache || !move || size <= 0 || cache->page_size_bits != 14 ||
		cache->page_count <= 0 || cache->page_count > HALO_PORT_TEXTURE_CACHE_PAGE_COUNT)
		return;
	needed = ((unsigned long)size + 0x3fffUL) >> 14;
	available = free_pages = cache->page_count;
	for (index = cache->first_block_index; index != NONE; index = block->next_block_index)
	{
		block = datum_get(cache->blocks, index);
		/* Preflight before any eviction or move. Async destinations stay fixed. */
		if (cache->locked_block_proc && cache->locked_block_proc(index))
		{
			result->status = 2;
			return;
		}
		free_pages -= block->page_count;
		if (block->last_used_tick == cache->tick)
			available -= block->page_count;
	}
	if (available < needed)
		return;
	/* Evict only as much old data as necessary, oldest first. Never relax
	 * the current-frame lock: earlier stages can already reference headers. */
	while (free_pages < needed)
	{
		oldest = NONE;
		oldest_tick = 0;
		for (index = cache->first_block_index; index != NONE; index = block->next_block_index)
		{
			block = datum_get(cache->blocks, index);
			if (block->last_used_tick != cache->tick &&
				(oldest == NONE || (unsigned long)block->last_used_tick < oldest_tick))
			{
				oldest = index;
				oldest_tick = (unsigned long)block->last_used_tick;
			}
		}
		if (oldest == NONE)
			return;
		block = datum_get(cache->blocks, oldest);
		free_pages += block->page_count;
		lruv_block_delete(cache, oldest);
		result->evicted_blocks++;
	}
	/* Address order is retained; downward memmove never overwrites the next
	 * live source. IDs, linked-list order, ages and header pointers survive. */
	destination = 0;
	for (index = cache->first_block_index; index != NONE; index = block->next_block_index)
	{
		block = datum_get(cache->blocks, index);
		if (block->first_page_index != destination)
		{
			move(index, destination, block->page_count);
			block->first_page_index = destination;
			result->moved_pages += block->page_count;
			result->moved_blocks++;
		}
		destination += block->page_count;
	}
	result->status = 1;
}
#endif
