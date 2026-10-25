/* Appended only to the diagnostic backend; no production API or state. */
void lighting_resource_diagnostics(double seconds)
{
	static FILE *file;
	unsigned long i, programs = 0, fragments = 0, targets = 0;
	struct render_target_entry *target;
	for (i = 0; i < PROGRAM_BUCKETS; i++)
	{
		struct program_entry *entry;
		for (entry = program_buckets[i]; entry; entry = entry->next) programs++;
	}
	for (i = 0; i < FRAGMENT_BUCKETS; i++)
	{
		struct fragment_entry *entry;
		for (entry = fragment_buckets[i]; entry; entry = entry->next) fragments++;
	}
	for (target = render_targets; target; target = target->next) targets++;
	if (!file)
	{
		file = fopen("resources.csv", "w");
		if (file) fprintf(file, "seconds,programs,fragments,targets,immediate_capacity\n");
	}
	if (file)
	{
		fprintf(file, "%.3f,%lu,%lu,%lu,%lu\n", seconds, programs, fragments, targets, device.immediate_capacity);
		fflush(file);
	}
}
