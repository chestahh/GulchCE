/* Real allocations in the stock object pool, held by stable references so
 * normal compaction can relocate them. No pool size or failure return edits. */
extern int m0_stress_pressure_target(long index);
extern long m0_fault_epoch(void);
static void *m0_pressure_blocks[1024];
static short m0_pressure_count;
static long m0_pressure_start = NONE;
static boolean m0_pressure_done;

void m0_pressure_release(void)
{
	long before;
	short count = m0_pressure_count;
	if (m0_pressure_start == NONE || m0_pressure_done) return;
	before = memory_pool_get_free_size(object_memory_pool);
	while (m0_pressure_count)
		memory_pool_block_free(object_memory_pool, &m0_pressure_blocks[--m0_pressure_count]);
	m0_pressure_done = TRUE;
	m0_emit("pressure_release", game_time_get(), m0_fault_epoch(), count, before,
		memory_pool_get_free_size(object_memory_pool), 0);
}

void m0_pressure_update(void)
{
	if (m0_pressure_start != NONE && !m0_pressure_done &&
		(m0_fault_epoch() != 2 || game_time_get() - m0_pressure_start >= 450))
		m0_pressure_release();
}

static void m0_pressure_before(long index)
{
	long size;
	if (m0_pressure_done || !m0_stress_pressure_target(index)) return;
	if (m0_pressure_start == NONE) m0_pressure_start = game_time_get();
	/* Fill through the allocator itself, down to its smallest useful block.
	 * Refill space freed by normal cleanup until the bounded hold ends. */
	for (size = 16384; size >= 4; size /= 2)
		while (m0_pressure_count < NUMBEROF(m0_pressure_blocks) &&
			memory_pool_block_allocate(object_memory_pool, &m0_pressure_blocks[m0_pressure_count], size))
			m0_pressure_count++;
	m0_emit("pressure_fill", game_time_get(), m0_fault_epoch(), index, m0_pressure_count,
		memory_pool_get_free_size(object_memory_pool), memory_pool_get_contiguous_free_size(object_memory_pool));
}
