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
	for (i = 0; i < sizeof(lengths)/sizeof(lengths[0]); i++)
	{
		struct nv2a_vertex_lighting lighting;
		int point = nv2a_vertex_shader_lighting(programs[i], lengths[i], &lighting) && lighting.lights == 2;
		char *shader = nv2a_vertex_shader_to_glsl(programs[i], lengths[i], 0, NULL);
		if (point)
		{
			recognized++;
			CHECK(strstr(shader, "if (plasma_model_slots[0] > 0.5)"), "first point light color reached in program %u", i);
			CHECK(strstr(shader, "if (plasma_model_slots[1] > 0.5)"), "second point light color reached in program %u", i);
			CHECK(strstr(shader, "vec3 plasma_model_position") < strstr(shader, "B.xyz *="), "position captured before color use");
		}
		else CHECK(!strstr(shader, "plasma_model_slots"), "non-point programs untouched");
		free(shader);
	}
	CHECK(recognized >= 2, "real two-point-light programs tested");
	return 0;
}
