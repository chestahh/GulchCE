/* Desktop test build only. Sample draws every 32nd frame; never wait for GPU
   results. A full ring skips samples instead of stalling the rendering thread.
   TIME_ELAPSED semantics: Khronos ARB_timer_query, independently implemented. */
enum { LIGHTING_GPU_QUERIES = 4096 };
static struct lighting_gpu_sample
{
	GLuint query;
	unsigned long frame;
	double seconds;
	int kind;
} lighting_gpu_samples[LIGHTING_GPU_QUERIES];
static unsigned long lighting_gpu_read, lighting_gpu_write, lighting_gpu_frame_number;
static int lighting_gpu_active;
static FILE *lighting_gpu_log;
double lighting_performance_seconds(void);

static void lighting_gpu_begin(void)
{
	struct lighting_gpu_sample *sample;
	if (lighting_gpu_frame_number % 32 || lighting_gpu_write - lighting_gpu_read == LIGHTING_GPU_QUERIES)
		return;
	sample = &lighting_gpu_samples[lighting_gpu_write % LIGHTING_GPU_QUERIES];
	if (!sample->query) glGenQueries(1, &sample->query);
	sample->frame = lighting_gpu_frame_number;
	sample->seconds = lighting_performance_seconds();
	sample->kind = lighting_profile_world ? lighting_profile_world : lighting_profile_model;
	glBeginQuery(GL_TIME_ELAPSED, sample->query);
	lighting_gpu_active = 1;
}

static void lighting_gpu_end(void)
{
	if (!lighting_gpu_active) return;
	glEndQuery(GL_TIME_ELAPSED);
	lighting_gpu_active = 0;
	lighting_gpu_write++;
}

static void lighting_gpu_frame(unsigned long frame)
{
	if (!lighting_gpu_log)
	{
		lighting_gpu_log = fopen("gpu.csv", "w");
		if (lighting_gpu_log)
		{
			setvbuf(lighting_gpu_log, NULL, _IOFBF, 65536);
			fprintf(lighting_gpu_log, "frame,seconds,kind,nanoseconds\n");
		}
	}
	while (lighting_gpu_read != lighting_gpu_write)
	{
		struct lighting_gpu_sample *sample = &lighting_gpu_samples[lighting_gpu_read % LIGHTING_GPU_QUERIES];
		GLuint ready, nanoseconds;
		glGetQueryObjectuiv(sample->query, GL_QUERY_RESULT_AVAILABLE, &ready);
		if (!ready) break;
		glGetQueryObjectuiv(sample->query, GL_QUERY_RESULT, &nanoseconds);
		if (lighting_gpu_log)
			fprintf(lighting_gpu_log, "%lu,%.6f,%d,%u\n", sample->frame, sample->seconds, sample->kind, nanoseconds);
		lighting_gpu_read++;
	}
	if (lighting_gpu_log) fflush(lighting_gpu_log);
	lighting_gpu_frame_number = frame + 1;
}
