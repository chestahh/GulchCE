/* Independently authored surface lattice and travelling electrical patches.
The undeformed model position keeps the pattern attached during skinning;
the original plasma vertex shader supplies its view-dependent rim. */

#include "xgpu.h"
#include "shield_look.h"

#include <string.h>

static int shield_look_draw;
static float shield_look_draw_uniforms[12];

void shield_look_set(float const *state, float const *color, float const *rim)
{
	shield_look_draw = state != NULL;
	if (state)
	{
		memcpy(shield_look_draw_uniforms, state, 4 * sizeof(float));
		memcpy(shield_look_draw_uniforms + 4, color, 4 * sizeof(float));
		memcpy(shield_look_draw_uniforms + 8, rim, 4 * sizeof(float));
	}
}

int shield_look_active(void)
{
	return shield_look_draw;
}

float const *shield_look_uniforms(void)
{
	return shield_look_draw_uniforms;
}

void shield_look_emit(struct xgpu_text *text)
{
	xgpu_text_append(text,
		"\tvec3 sp = xShieldPosition * mix(92.0, 460.0, shield_look_rim.w);\n"
		"\tfloat st = shield_look_state.x;\n"
		/* Three intersecting planes avoid a stretched pattern on the sides
		of the arms. Derivative filtering preserves the fine lines at range. */
		"\tvec3 sg = abs(fract(vec3(sp.x + sp.y * 0.57735, sp.x - sp.y * 0.57735, sp.z + sp.y * 0.57735)) - 0.5);\n"
		"\tfloat sd = min(sg.x, min(sg.y, sg.z));\n"
		"\tfloat sw = max(fwidth(sd), 0.018);\n"
		"\tfloat lattice = 1.0 - smoothstep(0.035, 0.035 + sw, sd);\n"
		"\tvec3 ep = xShieldPosition * mix(31.0, 90.0, shield_look_rim.w);\n"
		"\tfloat field = sin(ep.x * 1.7 + sin(ep.z * 1.3 + st * 3.7))\n"
		"\t\t+ sin(ep.y * 2.1 - ep.z + st * 2.9) * 0.65\n"
		"\t\t+ sin(ep.x * 3.4 + ep.y * 2.7 - st * 7.0) * 0.22;\n"
		"\tfloat arcwidth = max(fwidth(field), 0.035);\n"
		"\tfloat arc = 1.0 - smoothstep(0.025, 0.025 + arcwidth, abs(field));\n"
		"\tfloat shield_patch = smoothstep(-0.2, 0.65, sin(ep.z * 0.65 - ep.x * 0.3 + st * 2.4));\n"
		"\tfloat hit = shield_look_state.y;\n"
		"\tfloat recharge = shield_look_state.z;\n"
		"\tfloat depleted = shield_look_state.w;\n"
		"\tfloat ad = shield_look_rim.y - shield_look_rim.x;\n"
		"\tfloat rim = abs(ad * shield_look_rim.z) > 0.0001\n"
		"\t\t? clamp((xD0.a - shield_look_rim.x * shield_look_rim.z) / (ad * shield_look_rim.z), 0.0, 1.0) : 0.25;\n"
		"\trim = rim * rim;\n"
		"\tfloat shell = max(hit, recharge * 0.72) * (1.0 - depleted);\n"
		"\tfloat electricity = arc * shield_patch * (hit * 0.55 + recharge * 0.9 + depleted * 0.7);\n"
		"\tfloat energy = shell * (0.07 + lattice * 0.34 + rim * 0.8) + electricity;\n"
		/* The arms are close enough that a body-sized rim overwhelms their
		surface; use the same pattern with a fainter shell there. */
		"\tenergy *= mix(1.0, 0.74, shield_look_rim.w) * shield_look_color.a;\n"
		"\tvec3 hue = shield_look_color.rgb / max(max(shield_look_color.r, max(shield_look_color.g, shield_look_color.b)), 1.0);\n"
		"\tfloat brightness = min(max(shield_look_color.r, max(shield_look_color.g, shield_look_color.b)), 1.0);\n"
		"\tresult = vec4(mix(hue, vec3(brightness), clamp(rim * shell * 0.5 + electricity * 0.35, 0.0, 0.7)), clamp(energy, 0.0, 0.95));\n");
}
