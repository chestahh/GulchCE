/* Test-only collision clocks. Categories overlap; do not add inclusive totals. */
int lighting_trace_active;
static double collision_seconds[8];
static unsigned long collision_calls[8];
void lighting_resource_diagnostics(double seconds);
void lighting_texture_diagnostics(double seconds);

void lighting_trace_time(int category, __int64 started)
{
	LARGE_INTEGER now, frequency;
	if (!lighting_trace_active || !started || category < 0 || category >= 8) return;
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);
	collision_seconds[category] += (double)(now.QuadPart - started) / (double)frequency.QuadPart;
	collision_calls[category]++;
}

void lighting_diagnostics_frame(double seconds)
{
	static FILE *file;
	int i;
	if (!file)
	{
		file = fopen("collision.csv", "w");
		if (file) fprintf(file, "seconds,category,ms,calls\n");
	}
	if (file)
	{
		for (i = 0; i < 8; i++)
		{
			fprintf(file, "%.3f,%d,%.6f,%lu\n", seconds, i, collision_seconds[i]*1000, collision_calls[i]);
			collision_seconds[i] = 0;
			collision_calls[i] = 0;
		}
		fflush(file);
	}
	lighting_resource_diagnostics(seconds);
	lighting_texture_diagnostics(seconds);
}
