/* Same rays, pose, flags and ordering for both paths. No simulation step can
   intervene. Clocks surround whole batches; output comparison is not timed. */
#include <stdio.h>
static void trace_depths(real extent, real_point3d const *from, long ignore, int vehicles,
	real depths[6][OCC_SIZE * OCC_SIZE])
{
	static FILE *file;
	static unsigned long rays, mismatches, last, batches;
	static double fast_ticks, stock_ticks;
	real reference[6][OCC_SIZE * OCC_SIZE];
	LARGE_INTEGER begin, middle, end, frequency;
	unsigned long now;
	int face, cell;
	QueryPerformanceCounter(&begin);
	trace_depths_actual(extent, from, ignore, vehicles, depths);
	QueryPerformanceCounter(&middle);
	trace_depths_reference(extent, from, ignore, vehicles, reference);
	QueryPerformanceCounter(&end);
	fast_ticks += (double)(middle.QuadPart-begin.QuadPart);
	stock_ticks += (double)(end.QuadPart-middle.QuadPart);
	batches++;
	for (face = 0; face < 6; face++)
		for (cell = 0; cell < OCC_SIZE * OCC_SIZE; cell++)
		{
			rays++;
			if (depths[face][cell] != reference[face][cell])
			{
				mismatches++;
				if (mismatches <= 10) fprintf(stderr, "TRACE mismatch %.9g %.9g face %d cell %d\n",
					depths[face][cell], reference[face][cell], face, cell);
			}
		}
	if (!file)
	{
		file = fopen("trace-check.csv", "w");
		if (file) fprintf(file, "milliseconds,rays,mismatches,batches,fast_ms,stock_ms\n");
	}
	now = GetTickCount();
	if (file && now-last >= 1000)
	{
		QueryPerformanceFrequency(&frequency);
		fprintf(file, "%lu,%lu,%lu,%lu,%.6f,%.6f\n", now, rays, mismatches, batches,
			fast_ticks*1000/(double)frequency.QuadPart, stock_ticks*1000/(double)frequency.QuadPart);
		fflush(file);
		last = now;
	}
}
