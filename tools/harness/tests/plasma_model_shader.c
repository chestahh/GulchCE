#include "harness.h"
#include <stdarg.h>
typedef unsigned long DWORD;
typedef int BOOL;
#define XGPU_VERTEX_ATTRIBUTE_COUNT 16
#define XGPU_VERTEX_CONSTANT_COUNT 192
#define XGPU_VERTEX_CONSTANT_BIAS 96
struct xgpu_text;
void xgpu_text_append(struct xgpu_text *text, const char *format, ...);
#include "under_test.inc"
void xgpu_text_append(struct xgpu_text *text, const char *format, ...)
{
	char buffer[8192];
	va_list args;
	int length;
	va_start(args, format); length = vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
	CHECK(length >= 0 && length < sizeof(buffer), "bounded shader append");
	text->buffer = realloc(text->buffer, text->length + length + 1);
	memcpy(text->buffer + text->length, buffer, length + 1);
	text->length += length;
}
int main(int argc, char **argv)
{
	unsigned int i, recognized = 0;
	struct xgpu_text pixel = {0};
	for (i = 0; i < sizeof(lengths)/sizeof(lengths[0]); i++)
	{
		struct nv2a_vertex_lighting lighting;
		int point = nv2a_vertex_shader_lighting(programs[i], lengths[i], &lighting) && lighting.lights == 2;
		char *shader = nv2a_vertex_shader_to_glsl(programs[i], lengths[i], 0, NULL);
		if (point)
		{
			recognized++;
#ifndef HALO_ANDROID
			CHECK(strstr(shader, "if (plasma_model_slots[0] > 0.5)"), "first point light color reached in program %u", i);
			CHECK(strstr(shader, "if (plasma_model_slots[1] > 0.5)"), "second point light color reached in program %u", i);
			CHECK(strstr(shader, "vec3 plasma_model_position") < strstr(shader, "B.xyz *="), "position captured before color use");
#endif
		}
		else CHECK(!strstr(shader, "plasma_model_slots"), "non-point programs untouched");
#ifdef HALO_ANDROID
		CHECK(!strstr(shader, "plasma_") && !strstr(shader, "sampler3D"), "Android vertex program has no optional lighting");
#endif
		free(shader);
	}
	CHECK(recognized >= 2, "real two-point-light programs tested");
	model_lighting(&pixel, 1);
#ifdef HALO_ANDROID
	CHECK(!strstr(pixel.buffer, "plasma_") && !strstr(pixel.buffer, "sampler3D"), "Android per-pixel lighting is stock");
	CHECK(strstr(pixel.buffer, "light += max(1.0 - distance_squared * model_lights[1].w, 0.0)"), "first stock point light");
	CHECK(strstr(pixel.buffer, "light += max(1.0 - distance_squared * model_lights[4].w, 0.0)"), "second stock point light");
#else
	CHECK(strstr(pixel.buffer, "plasma_model_falloff") && strstr(pixel.buffer, "plasma_model_slots[1]"), "supported per-pixel lighting retained");
#endif
	free(pixel.buffer);
	memset(&pixel, 0, sizeof(pixel));
	model_lighting(&pixel, 0);
	CHECK(!strstr(pixel.buffer, "plasma_") && !strstr(pixel.buffer, "xWorldPosition"), "ambient-only models unchanged");
	free(pixel.buffer);
	return 0;
}
