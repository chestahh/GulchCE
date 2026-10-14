/* The selected point light gets (1 - d*d/r*r)^2. All other contributions
   and the stock shader's alpha, cone and material terms are retained. */
#include "xgpu.h"
#include "plasma_model_shader.h"

void plasma_model_emit_uniform(struct xgpu_text *text)
{
	xgpu_text_append(text, "uniform vec2 plasma_model_slots;\n");
}

void plasma_model_emit_vertex(struct xgpu_text *text, const unsigned long *instruction,
	unsigned long index, const struct nv2a_vertex_lighting *lighting)
{
	unsigned long constant = (instruction[1] >> 13) & 255;
	int slot;
	if (index == lighting->position_instruction)
		xgpu_text_append(text, "\tvec3 plasma_model_position = r%lu.xyz;\n", lighting->position_register);
	/* Only the color multiply-add of a recognized two-point-light model
	   program: B is the unswizzled, positive color constant. The ILU reads C,
	   so changing B.xyz cannot affect the parallel scalar instruction. */
	if (((instruction[1] >> 21) & 15) != 4 || (instruction[3] & 2) ||
		((instruction[2] >> 11) & 3) != 3 || ((instruction[2] >> 25) & 1) ||
		((instruction[2] >> 19) & 63) != 6 || index <= lighting->position_instruction)
		return;
	for (slot = 0; slot < 2; slot++)
		if (constant == (unsigned long)(XGPU_VERTEX_CONSTANT_BIAS - 77 + slot * 3))
			xgpu_text_append(text,
				"\tif (plasma_model_slots[%d] > 0.5) {\n"
				"\t\tvec3 delta = c[%d].xyz - plasma_model_position;\n"
				"\t\tB.xyz *= max(1.0 - dot(delta, delta) * c[%d].w, 0.0);\n\t}\n",
				slot, XGPU_VERTEX_CONSTANT_BIAS - 79 + slot * 3, XGPU_VERTEX_CONSTANT_BIAS - 79 + slot * 3);
}

void plasma_model_emit_pixel(struct xgpu_text *text)
{
	plasma_model_emit_uniform(text);
	xgpu_text_append(text,
		"float plasma_model_falloff(float attenuation, float selected) {\n"
		"\treturn selected > 0.5 ? attenuation * attenuation : attenuation;\n}\n");
}
