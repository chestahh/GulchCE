#include "geometry_diagnostics.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#if __has_include("geometry_build.h")
#include "geometry_build.h"
#else
#define GEOMETRY_BUILD "local"
#endif

/* No network upload. Keep the prior launch and cap this launch at 8 MiB. */
#define GEOMETRY_LOG_LIMIT (8UL * 1024UL * 1024UL)
static FILE *geometry_file;
static unsigned long geometry_bytes;
static pthread_mutex_t geometry_lock = PTHREAD_MUTEX_INITIALIZER;

void host_geometry_log(const char *line)
{
	size_t length;

	pthread_mutex_lock(&geometry_lock);
	length = strlen(line);
	if (geometry_file && geometry_bytes + length + 1 <= GEOMETRY_LOG_LIMIT - 64)
	{
		fwrite(line, 1, length, geometry_file);
		fputc('\n', geometry_file);
		fflush(geometry_file);
		geometry_bytes += (unsigned long)length + 1;
	}
	else if (geometry_file)
	{
		fputs("geometry-test: log limit reached; capture stopped\n", geometry_file);
		fclose(geometry_file);
		geometry_file = NULL;
	}
	pthread_mutex_unlock(&geometry_lock);
}

void host_geometry_event(const char *format, ...)
{
	char line[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	host_geometry_log(line);
}

void host_geometry_begin(const char *root)
{
	char path[1024], previous[1040];
	int length;

	pthread_mutex_lock(&geometry_lock);
	if (geometry_file)
		fclose(geometry_file);
	geometry_file = NULL;
	geometry_bytes = 0;
	length = snprintf(path, sizeof(path), "%s/android-geometry-test.log", root);
	if (length >= 0 && (size_t)length < sizeof(path))
	{
		snprintf(previous, sizeof(previous), "%s.previous", path);
		remove(previous);
		rename(path, previous);
		geometry_file = fopen(path, "wb");
	}
	pthread_mutex_unlock(&geometry_lock);
	host_geometry_event("geometry-test: mode=geometry-diagnostic-v1 commit=%s compiled=%s %s", GEOMETRY_BUILD, __DATE__, __TIME__);
	host_geometry_log("geometry-test: diagnostics only; geometry bypass, normal texture caching, 128MiB CE cache and state refresh retained");
#ifdef __ANDROID__
	{
		static const char *names[] = { "ro.product.model", "ro.hardware", "ro.build.version.release", "ro.build.version.sdk" };
		unsigned int i;
		char value[PROP_VALUE_MAX];
		for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		{
			value[0] = 0;
			__system_property_get(names[i], value);
			host_geometry_event("geometry-test: %s=%s", names[i], value);
		}
	}
#endif
}
