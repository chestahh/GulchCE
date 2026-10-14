/* Additional model samplers use units 6/7; the game's four texture stages
   and their cached bindings are not changed. */
#include "plasma_occlusion.h"
#include "xgpu.h"
#include <stdio.h>

void plasma_occlusion_bind_model(unsigned long program)
{
	GLint active, location;
	GLuint texture;
	GLenum target;
	struct xgpu_texture_description description;
	DWORD const *resource;
	int slot;
	float transform[4], enabled[2] = {0, 0};
	char name[40];
	static GLuint white;
	location = glGetUniformLocation(program, "plasma_occ_enabled");
	if (location < 0)
		return;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	for (slot = 0; slot < 2; slot++)
	{
		glActiveTexture(GL_TEXTURE0 + 6 + slot);
		resource = plasma_occlusion_model_texture(slot, transform);
		texture = 0;
		if (resource)
			texture = xgpu_texture_get(resource, NULL, &target, &description);
		glBindSampler(6 + slot, 0);
		if (texture)
		{
			enabled[slot] = 1;
			glBindTexture(GL_TEXTURE_3D, texture);
			snprintf(name, sizeof(name), "plasma_occ_transform%d", slot);
			glUniform4fv(glGetUniformLocation(program, name), 1, transform);
		}
		else
		{
			if (!white)
			{
				unsigned char pixel[4] = {255, 255, 255, 255};
				glGenTextures(1, &white);
				glBindTexture(GL_TEXTURE_3D, white);
				glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			}
			glBindTexture(GL_TEXTURE_3D, white);
		}
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		snprintf(name, sizeof(name), "plasma_occ_texture%d", slot);
		glUniform1i(glGetUniformLocation(program, name), 6 + slot);
	}
	glUniform2f(location, enabled[0], enabled[1]);
	glActiveTexture(active);
}
