/* Procedural shield surface. Included by the pixel shader translator; a zero
scale leaves the original fragment untouched. One grid follows the model UVs,
without blending overlapping projections. Filter from the unfolded coordinates:
the distance field folds at shared edges, where its derivatives can cancel.
Pulse continuously across shared links, with a faint persistent core. No extracted
Infinite textures or shader code. */
#ifndef SHIELD_HONEYCOMB_GL_H
#define SHIELD_HONEYCOMB_GL_H

static const char shield_honeycomb_glsl[] =
	"in vec2 shield_uv;\n"
	"uniform vec4 shield_honeycomb[3];\n"
	"float shield_hex(vec2 p, float time)\n"
	"{\n"
	"    vec2 period = vec2(1.7320508, 3.0);\n"
	"    vec2 a = mod(p, period) - period * 0.5;\n"
	"    vec2 b = mod(p - period * 0.5, period) - period * 0.5;\n"
	"    vec2 q = dot(a,a) < dot(b,b) ? a : b;\n"
	"    q = abs(q);\n"
	"    float edge = 0.8660254 - max(q.x, dot(q, vec2(0.5,0.8660254)));\n"
	"    float footprint = max(length(dFdx(p)),length(dFdy(p)));\n"
	"    float aa = clamp(footprint*0.5,0.002,0.25);\n"
	"    float line = 1.0 - smoothstep(0.07-aa,0.07+aa,edge);\n"
	"    float glow = exp(-max(edge,0.0)*9.0);\n"
	"    float pulse = pow(0.5+0.5*sin(dot(p,vec2(1.7,2.3))+time*2.2),4.0);\n"
	"    float pattern = 0.015+line*(0.28+0.37*pulse)+glow*(0.10+0.20*pulse);\n"
	"    return mix(pattern,0.13,smoothstep(0.5,1.5,footprint));\n"
	"}\n"
	"vec4 shield_honeycomb_surface(vec4 original)\n"
	"{\n"
	"    if (shield_honeycomb[0].w == 0.0) return original;\n"
	"    float time = shield_honeycomb[1].w;\n"
	"    float pattern = shield_hex(shield_uv * shield_honeycomb[0].w, time);\n"
	"    float energy = shield_honeycomb[2].a*xD0.a;\n"
	"    float membrane = min(max(original.a,0.0),energy)*0.04;\n"
	"    return vec4(shield_honeycomb[2].rgb + shield_honeycomb[1].rgb*0.65,energy*pattern+membrane);\n"
	"}\n";

#endif
