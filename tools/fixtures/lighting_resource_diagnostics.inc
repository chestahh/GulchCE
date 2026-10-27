/* Appended only to the diagnostic backend; no production API or state. */
void lighting_resource_diagnostics(double seconds)
{
	static FILE *file;
	unsigned long i, programs = 0, fragments = 0, targets = 0;
	unsigned long sources = 0, source_bytes = 0;
	unsigned long vertex_arrays = 0, mirror_buffers = 0, framebuffer_count = 0;
	struct render_target_entry *target;
	struct framebuffer_entry *framebuffer;
	for (i = 0; i < PROGRAM_BUCKETS; i++)
	{
		struct program_entry *entry;
		for (entry = program_buckets[i]; entry; entry = entry->next) programs++;
	}
	for (i = 0; i < FRAGMENT_BUCKETS; i++)
	{
		struct fragment_entry *entry;
		struct fragment_source_entry *source;
		for (entry = fragment_buckets[i]; entry; entry = entry->next) fragments++;
		for (source = fragment_source_buckets[i]; source; source = source->next)
		{
			sources++;
			source_bytes += strlen(source->source) + 1;
		}
	}
	for (target = render_targets; target; target = target->next) targets++;
	for (framebuffer = framebuffers; framebuffer; framebuffer = framebuffer->next) framebuffer_count++;
	for (i = 0; i < VERTEX_ARRAY_BUCKET_COUNT; i++)
	{
		struct vertex_array_entry *entry;
		for (entry = vertex_array_buckets[i]; entry; entry = entry->next) vertex_arrays++;
	}
	for (i = 0; i < MIRROR_SEGMENT_COUNT; i++) mirror_buffers += mirror.buffers[i] != 0;
	if (!file)
	{
		file = fopen("resources.csv", "w");
		if (file) fprintf(file, "seconds,programs,fragments,targets,immediate_capacity,fragment_sources,fragment_source_bytes,vertex_arrays,samplers,mirror_buffers,framebuffers\n");
	}
	if (file)
	{
		fprintf(file, "%.3f,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", seconds, programs, fragments, targets, device.immediate_capacity, sources, source_bytes,
			vertex_arrays, sampler_cache_count, mirror_buffers, framebuffer_count);
		fflush(file);
	}
}
