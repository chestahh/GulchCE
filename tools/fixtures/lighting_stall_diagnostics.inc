/* Windows diagnostic build only; no production API, state or GL queries.
   GetThreadTimes reports CPU execution time in 100 ns units, not wall time:
   https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes
   Aggregate CPU deltas over many frames. The processor is sampled at the
   frame boundary. QPC seconds align it with external hardware samples. */
__declspec(dllimport) BOOL WINAPI GetThreadTimes(HANDLE, struct _FILETIME *, struct _FILETIME *, struct _FILETIME *, struct _FILETIME *);
__declspec(dllimport) DWORD WINAPI GetCurrentProcessorNumber(void);
static Uint64 present_started, present_ticks[4];

void lighting_stall_present_begin(void)
{
	present_started = SDL_GetPerformanceCounter();
}

void lighting_stall_present_end(int phase)
{
	present_ticks[phase] += SDL_GetPerformanceCounter() - present_started;
}

static double stall_shader_started[2], stall_shader_ms[2];
static unsigned long stall_shader_calls[2];

void lighting_stall_shader_begin(int kind)
{
	stall_shader_started[kind] = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

void lighting_stall_shader_end(int kind)
{
	double now = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
	stall_shader_ms[kind] += (now - stall_shader_started[kind])*1000.0;
	stall_shader_calls[kind]++;
}

void lighting_stall_frame(double seconds, double frame_ms)
{
	static FILE *file;
	static double previous_cpu, flushed;
	struct _FILETIME created, exited, kernel, user;
	double cpu = -1, delta = -1;
	double frequency = (double)SDL_GetPerformanceFrequency();
	double timestamp = (double)SDL_GetPerformanceCounter() / frequency;
	unsigned long processor = GetCurrentProcessorNumber();
	int phase;
	if (GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
	{
		cpu = ((double)kernel.dwHighDateTime*4294967296.0 + kernel.dwLowDateTime +
			(double)user.dwHighDateTime*4294967296.0 + user.dwLowDateTime) / 10000.0;
		if (previous_cpu > 0) delta = cpu - previous_cpu;
	}
	if (!file)
	{
		file = fopen("stalls.csv", "w");
		if (file)
		{
			setvbuf(file, NULL, _IOFBF, 65536);
			fprintf(file, "seconds,frame_ms,thread_cpu_ms,compile_ms,link_ms,compiles,links,processor,qpc_s,query_copy_ms,swap_ms,pacing_wait_ms,query_read_ms\n");
		}
	}
	if (file)
	{
		fprintf(file, "%.6f,%.6f,%.6f,%.6f,%.6f,%lu,%lu,%lu,%.7f", seconds, frame_ms, delta,
			stall_shader_ms[0], stall_shader_ms[1], stall_shader_calls[0], stall_shader_calls[1], processor, timestamp);
		for (phase = 0; phase < 4; ++phase)
			fprintf(file, ",%.6f", (double)present_ticks[phase] * 1000.0 / frequency);
		fputc('\n', file);
		if (seconds-flushed >= 1) { fflush(file); flushed = seconds; }
	}
	for (phase = 0; phase < 4; ++phase)
		present_ticks[phase] = 0;
	previous_cpu = cpu;
	stall_shader_ms[0] = stall_shader_ms[1] = 0;
	stall_shader_calls[0] = stall_shader_calls[1] = 0;
}
