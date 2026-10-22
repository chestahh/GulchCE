#include "vulkan_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Existing translator output is consumed through its public interface.
   Only the Vulkan binding/interface declarations change; shader arithmetic
   stays in the established translator, which makes no OpenGL calls. */
char *halo_vk_shader_interface(const char *source, int fragment)
{
	static const char block[] =
		"#version 450\nlayout(std140,set=0,binding=0) uniform Halo {\n"
		"vec4 c[192]; vec4 viewport_scale; vec4 viewport_offset;\n"
		"float point_size; float screen_offset; float alpha_reference; float reserved;\n"
		"vec4 height_fade; vec4 ps_c0[8]; vec4 ps_c1[8];\n"
		"vec4 ps_final_c0; vec4 ps_final_c1; vec4 fog_color; vec4 fog_parameters;\n"
		"vec4 bump_matrix[4]; vec4 bump_luminance[4]; vec4 texture_scale[4];\n"
		"vec4 shield_honeycomb[3];\n};\n";
	static const char *varyings[] = {"xD0", "xD1", "xB0", "xB1", "xT0", "xT1", "xT2", "xT3", "xFog", "shield_uv"};
	size_t length, used = sizeof(block) - 1, n;
	char *result;
	const char *line = source, *end;
	unsigned i, binding;
	char type[32], name[32];
	(void)fragment;
	if (!source)
		return NULL;
	length = strlen(source);
	if (length > 1024 * 1024)
		return NULL;
	result = malloc(length * 4 + sizeof(block) + 2048);
	if (!result)
	{
		free(result);
		return NULL;
	}
	memcpy(result, block, used);
	while (*line)
	{
		end = strchr(line, '\n');
		n = end ? (size_t)(end - line + 1) : strlen(line);
		if (!strncmp(line, "#version", 8))
		{
			line += n;
			continue;
		}
		if (!strncmp(line, "uniform ", 8))
		{
			if (sscanf(line, "uniform %31s tex%u;", type, &binding) == 2 && binding < 4)
				used += sprintf(result + used, "layout(set=0,binding=%u) uniform %s tex%u;\n", binding + 1,
					type, binding);
			line += n;
			continue;
		}
		if (sscanf(line, "in %31s %31[^;];", type, name) == 2 ||
			sscanf(line, "out %31s %31[^;];", type, name) == 2)
			for (i = 0; i < sizeof(varyings) / sizeof(*varyings); i++)
				if (!strcmp(name, varyings[i]))
				{
					used += sprintf(result + used, "layout(location=%u) ", i);
					break;
				}
		memcpy(result + used, line, n);
		used += n;
		line += n;
	}
	result[used] = 0;
	return result;
}
