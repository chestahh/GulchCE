/* Appended only to the diagnostic backend; no production API or state. */
void lighting_resource_diagnostics(double seconds)
{
	static FILE *file;
	unsigned long i, programs = 0, fragments = 0, targets = 0;
	unsigned long sources = 0, source_bytes = 0;
	struct render_target_entry *target;
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
	if (!file)
	{
		file = fopen("resources.csv", "w");
		if (file) fprintf(file, "seconds,programs,fragments,targets,immediate_capacity,fragment_sources,fragment_source_bytes\n");
	}
	if (file)
	{
		fprintf(file, "%.3f,%lu,%lu,%lu,%lu,%lu,%lu\n", seconds, programs, fragments, targets, device.immediate_capacity, sources, source_bytes);
		fflush(file);
	}
}
