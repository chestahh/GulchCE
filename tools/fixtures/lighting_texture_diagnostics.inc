/* Guest payload sizes describe retained cache entries, not driver allocations. */
void lighting_texture_diagnostics(double seconds)
{
	static FILE *file;
	unsigned long i, count = 0, bytes = 0, volumes = 0, volume_bytes = 0;
	for (i = 0; i < TEXTURE_BUCKET_COUNT; i++)
	{
		struct texture_entry *entry;
		for (entry = texture_buckets[i]; entry; entry = entry->next)
		{
			count++; bytes += entry->size;
			if (entry->target == GL_TEXTURE_3D) { volumes++; volume_bytes += entry->size; }
		}
	}
	if (!file)
	{
		file = fopen("textures.csv", "w");
		if (file) fprintf(file, "seconds,count,guest_bytes,volumes,volume_guest_bytes\n");
	}
	if (file)
	{
		fprintf(file, "%.3f,%lu,%lu,%lu,%lu\n", seconds, count, bytes, volumes, volume_bytes);
		fflush(file);
	}
}
