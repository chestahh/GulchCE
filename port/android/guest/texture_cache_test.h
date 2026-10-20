/* Android graphics-state comparison diagnostics, included by xbox_textures.c.
The renderer owns this state. Files are local, bounded and flushed as written;
the previous launch is kept so restarting does not discard the last report. */
#include <stdarg.h>

#define ANDROID_TEXTURE_TEST_LOG_LIMIT (512UL * 1024UL)
static FILE *android_texture_test_file;
static unsigned long android_texture_test_log_bytes;
static unsigned long android_state_test_refreshes;
static Uint64 android_state_test_now, android_state_test_next_sample;
static BOOL android_state_test_sample;
static unsigned long android_texture_test_uploads, android_texture_test_decoded;
static unsigned long android_texture_test_errors;
static Uint64 android_texture_test_upload_ms, android_texture_test_next_summary;
static Uint64 android_texture_test_next_sample;
static BOOL android_texture_test_sample;

static void android_texture_test_log(const char *format, ...)
{
	char line[768];
	va_list arguments;
	size_t length;

	if (android_texture_test_log_bytes >= ANDROID_TEXTURE_TEST_LOG_LIMIT)
		return;
	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	length = strlen(line);
	android_texture_test_log_bytes += (unsigned long)length + 1;
	platform_log("state-test: %s", line);
	if (android_texture_test_file)
	{
		fprintf(android_texture_test_file, "%s\n", line);
		fflush(android_texture_test_file);
	}
}

static void android_texture_test_begin_frame(unsigned long frame)
{
	static BOOL initialized;
	Uint64 now = SDL_GetTicks();

	android_state_test_now = now;
	if (!initialized)
	{
		const char *root = getenv("HALO_DATA_ROOT");
		char path[1024], previous[1024];
		int length;

		initialized = TRUE;
		length = snprintf(path, sizeof(path), "%s/android-state-test.log", root && *root ? root : ".");
		if (length >= 0 && (size_t)length < sizeof(path))
		{
			length = snprintf(previous, sizeof(previous), "%s.previous", path);
			if (length >= 0 && (size_t)length < sizeof(previous))
			{
				remove(previous);
				rename(path, previous);
				android_texture_test_file = fopen(path, "w");
			}
		}
		android_texture_test_log("mode=state-refresh-each-draw; normal texture cache; geometry mirror bypass retained; "
			"texture decoding, uniforms and map adapters unchanged; file=%s",
			android_texture_test_file ? "ready" : "unavailable (logcat only)");
		android_texture_test_log("vendor=%s; renderer=%s; GL=%s; S3TC=%d; GLSL=%s",
			(const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER),
			(const char *)glGetString(GL_VERSION), xgpu_capabilities.s3tc, xgpu_capabilities.shading_language);
	}
	if (now >= android_texture_test_next_summary)
	{
		android_texture_test_log("ms=%llu frame=%lu totals: uploads=%lu state_refreshes=%lu decoded_DXT=%lu "
			"upload_CPU_ms=%llu sampled_GL_errors=%lu (upload and draw preparation sampled once/sec, not every call)",
			(unsigned long long)now, frame, android_texture_test_uploads, android_state_test_refreshes,
			android_texture_test_decoded, (unsigned long long)android_texture_test_upload_ms,
			android_texture_test_errors);
		android_texture_test_next_summary = now + 10000;
	}
}

static void android_texture_test_error(const char *where)
{
	GLenum error;
	unsigned int count;

	for (count = 0; count < 8 && (error = glGetError()) != GL_NO_ERROR; count++)
	{
		android_texture_test_errors++;
		android_texture_test_log("%s GL error=0x%x", where, (unsigned int)error);
	}
}

static Uint64 android_texture_test_upload_begin(void)
{
	Uint64 now = SDL_GetTicks();

	android_texture_test_sample = now >= android_texture_test_next_sample;
	if (android_texture_test_sample)
	{
		android_texture_test_next_sample = now + 1000;
		android_texture_test_error("before upload (earlier GL work)");
	}
	return now;
}

static void android_texture_test_upload_end(const struct xgpu_texture_description *description,
	BOOL decoded, unsigned char channels, Uint64 started)
{
	Uint64 elapsed = SDL_GetTicks() - started;

	android_texture_test_uploads++;
	android_texture_test_decoded += decoded != FALSE;
	android_texture_test_upload_ms += elapsed;
	if (android_texture_test_sample)
	{
		android_texture_test_error("after upload");
		android_texture_test_log("upload=%lu fmt=%02x size=%lux%lux%lu levels=%lu cube=%d "
			"compressed=%d decoded=%d channels=%u CPU_ms=%llu",
			android_texture_test_uploads, (unsigned int)description->format, description->width,
			description->height, description->depth, description->levels, description->cube_map,
			description->compressed, decoded, (unsigned int)channels, (unsigned long long)elapsed);
	}
}

/* This refresh deliberately does not touch texture contents or per-program
uniform caches. It isolates the GL shadow invalidation done by upload(). */
void android_state_test_refresh(unsigned long shader, unsigned long modes, BOOL immediate)
{
	android_state_test_refreshes++;
	android_state_test_sample = android_state_test_now >= android_state_test_next_sample;
	if (android_state_test_sample)
	{
		android_state_test_next_sample = android_state_test_now + 1000;
		android_texture_test_log("draw=%lu vertex_shader=%lu texture_modes=%08lx immediate=%d",
			android_state_test_refreshes, shader, modes, immediate);
		android_texture_test_error("before draw preparation (earlier GL work)");
	}
	xgpu_gl_state_invalidate();
}

void android_state_test_stage(const char *stage)
{
	if (android_state_test_sample)
	{
		android_texture_test_error(stage);
		android_texture_test_log("draw=%lu stage=%s", android_state_test_refreshes, stage);
	}
}

void android_state_test_texture(unsigned long stage, unsigned long data, unsigned long format)
{
	if (android_state_test_sample)
		android_texture_test_log("draw=%lu texture_stage=%lu data=%08lx format=%08lx",
			android_state_test_refreshes, stage, data, format);
}
