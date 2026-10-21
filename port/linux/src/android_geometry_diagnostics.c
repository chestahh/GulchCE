#include "android_geometry_diagnostics.h"
#ifdef HALO_GEOMETRY_DIAGNOSTICS
#include "xgpu.h"
#include <stdarg.h>
#include <stdio.h>

unsigned long geometry_test_frame, geometry_test_draw;
int geometry_test_sample = 1;
static unsigned long geometry_test_until = 60, geometry_test_errors;
static unsigned long geometry_test_lines;

void geometry_test_log(const char *format, ...)
{
	char line[1024];
	va_list args;
	if (geometry_test_lines++ >= 20000)
		return;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	platform_log("geometry-test: frame=%lu draw=%lu %s", geometry_test_frame, geometry_test_draw, line);
}

void geometry_test_map(const char *name)
{
	geometry_test_until = geometry_test_frame + 60;
	geometry_test_sample = 1;
	geometry_test_log("map=%s; detailed GL checks for next 60 frames, then one frame per 120", name);
}

void geometry_test_error(const char *where, int force)
{
	unsigned int i;
	GLenum error;
	if (!force && !geometry_test_sample)
		return;
	for (i = 0; i < 8 && (error = glGetError()) != GL_NO_ERROR; i++)
	{
		geometry_test_errors++;
		if (geometry_test_errors <= 128)
			geometry_test_log("GL error=%04x at=%s (since preceding check)", (unsigned int)error, where);
		/* A detected failure opens a short detailed window, with a bounded log. */
		if (geometry_test_errors <= 16)
			geometry_test_until = geometry_test_frame + 60;
	}
}

void geometry_test_present(unsigned long frame)
{
	static int identified;
	if (!identified)
	{
		identified = 1;
		geometry_test_log("GPU vendor=%s renderer=%s GL=%s GLSL=%s base_vertex=%d s3tc=%d",
			(const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER),
			(const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION),
			xgpu_capabilities.base_vertex, xgpu_capabilities.s3tc);
	}
	geometry_test_error("frame end", 1);
	if (!(frame % 120))
		geometry_test_log("frame summary draws=%lu GL_errors=%lu sampling=%d", geometry_test_draw, geometry_test_errors, geometry_test_sample);
	geometry_test_frame = frame + 1;
	geometry_test_draw = 0;
	geometry_test_sample = geometry_test_frame < geometry_test_until || !(geometry_test_frame % 120);
}
#endif
