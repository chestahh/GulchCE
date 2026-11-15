/* Feature-owned point-light depth maps. No stock render target or shader is
   modified. GLES has no shadow resources or shader variants. */
#include "modern_shadows.h"
#include "xgpu.h"
#include <stddef.h>
#include <string.h>

#ifndef HALO_GLES
enum { SHADOW_SIZE = 512, SHADOW_MESHES = 2048, SHADOW_DRAWS = 256,
	SHADOW_BYTES = 32 * 1024 * 1024, SHADOW_NODES = 64 };
static struct { GLuint vertices, indices; int count; } meshes[SHADOW_MESHES];
static struct
{
	int mesh, nodes, views;
	float matrices[SHADOW_NODES * 16], uv_transform[8];
	GLuint alpha;
} draws[SHADOW_DRAWS];
static int mesh_count, draw_count, mesh_bytes, failed, soft_filter, resource_failed;
static struct shadow_map
{
	GLuint depth_cube[2];
	float light[4];
	int valid, view_map;
} maps[MODERN_SHADOW_LIGHTS];
static struct shadow_map *map = &maps[0];
static GLuint framebuffer, program, vao, alpha_sampler;
static GLint light_uniform, face_uniform, bones_uniform, alpha_uniform, uv_uniform;
static int drawing;

static void create_depth_cube(GLuint *texture)
{
	int face;
	glGenTextures(1, texture);
	glActiveTexture(GL_TEXTURE0 + 8);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *texture);
	for (face = 0; face < 6; face++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT32F,
			SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

static int create_resources(void)
{
	static const char vertex[] =
		"#version 330 core\n"
		"layout(location=0) in vec3 position;\n"
		"layout(location=1) in vec2 nodes;\n"
		"layout(location=2) in float weight;\n"
		"layout(location=3) in vec2 uv;\n"
		"uniform mat4 bones[64]; uniform vec4 light; uniform int face;\n"
		"uniform vec4 uv_transform[2]; out vec3 delta; out vec2 texcoord;\n"
		"void main() {\n"
		" vec4 p = mix(bones[int(nodes.y)]*vec4(position,1), bones[int(nodes.x)]*vec4(position,1),weight);\n"
		" delta=p.xyz-light.xyz; texcoord=vec2(dot(vec4(uv,0,1),uv_transform[0]),dot(vec4(uv,0,1),uv_transform[1]));\n"
		" vec3 d=delta; vec3 v;\n"
		" if(face==0) v=vec3(-d.z,-d.y,d.x);\n"
		" else if(face==1) v=vec3(d.z,-d.y,-d.x);\n"
		" else if(face==2) v=vec3(d.x,d.z,d.y);\n"
		" else if(face==3) v=vec3(d.x,-d.z,-d.y);\n"
		" else if(face==4) v=vec3(d.x,-d.y,d.z);\n"
		" else v=vec3(-d.x,-d.y,-d.z);\n"
		" float n=0.005; float f=light.w;\n"
		" gl_Position=vec4(v.xy,((f+n)*v.z-2.0*f*n)/(f-n),v.z);\n}\n";
	static const char fragment[] =
		"#version 330 core\n"
		"in vec3 delta; in vec2 texcoord; uniform vec4 light;\n"
		"uniform sampler2D alpha_map; uniform int alpha_test;\n"
		"void main() {\n"
		" if(alpha_test!=0 && texture(alpha_map,texcoord).a <= (127.0/255.0)) discard;\n"
		" gl_FragDepth=min(length(delta)/light.w,1.0);\n}\n";
	GLuint vs, fs;
	GLenum none = GL_NONE;
	if (program) return 1;
	vs = xgpu_compile_shader(GL_VERTEX_SHADER, vertex, "modern shadow caster");
	fs = xgpu_compile_shader(GL_FRAGMENT_SHADER, fragment, "modern shadow depth");
	program = vs && fs ? xgpu_link_program(vs, fs, "modern shadow depth") : 0;
	if (vs) glDeleteShader(vs);
	if (fs) glDeleteShader(fs);
	if (!program) return 0;
	glGenVertexArrays(1, &vao);
	glGenSamplers(1, &alpha_sampler);
	glSamplerParameteri(alpha_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(alpha_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(alpha_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(alpha_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glDrawBuffers(1, &none);
	glReadBuffer(GL_NONE);
	light_uniform = glGetUniformLocation(program, "light");
	face_uniform = glGetUniformLocation(program, "face");
	bones_uniform = glGetUniformLocation(program, "bones");
	alpha_uniform = glGetUniformLocation(program, "alpha_test");
	uv_uniform = glGetUniformLocation(program, "uv_transform");
	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "alpha_map"), 9);
	return 1;
}

int modern_shadow_begin(int slot, const float position[3], float radius, int soft)
{
	if (slot < 0 || slot >= MODERN_SHADOW_LIGHTS || drawing) return 0;
	map = &maps[slot];
	map->valid = 0;
	draw_count = 0;
	if (!xgpu_gl_ready() || radius <= 0.01f || resource_failed) return 0;
	drawing = 1;
	failed = !create_resources();
	if (!failed && !map->depth_cube[0])
	{
		create_depth_cube(&map->depth_cube[0]);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, map->depth_cube[0], 0);
		failed = glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR;
	}
	memcpy(map->light, position, 3 * sizeof(float));
	map->light[3] = radius;
	soft_filter = soft;
	/* A failed allocation still hands state back through end(). */
	if (failed)
	{
		resource_failed = 1;
		platform_log("GPU: modern shadow resources unavailable; retaining existing lighting until reset");
		modern_shadow_end();
		return 0;
	}
	return 1;
}

int modern_shadow_mesh_create(const struct modern_shadow_vertex *vertices, int vertex_count,
	const unsigned short *indices, int index_count)
{
	int size, index;
	if (!drawing || vertex_count < 1 || vertex_count > 65536 || index_count < 1 || index_count > 196608 ||
		mesh_count == SHADOW_MESHES) return 0;
	size = vertex_count * sizeof(*vertices) + index_count * sizeof(*indices);
	if (size > SHADOW_BYTES - mesh_bytes) return 0;
	for (index = 0; index < index_count; index++) if (indices[index] >= vertex_count) return 0;
	glBindVertexArray(vao);
	glGenBuffers(1, &meshes[mesh_count].vertices);
	glBindBuffer(GL_ARRAY_BUFFER, meshes[mesh_count].vertices);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(*vertices), vertices, GL_STATIC_DRAW);
	glGenBuffers(1, &meshes[mesh_count].indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mesh_count].indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(*indices), indices, GL_STATIC_DRAW);
	if (glGetError() != GL_NO_ERROR)
	{
		glDeleteBuffers(1, &meshes[mesh_count].vertices);
		glDeleteBuffers(1, &meshes[mesh_count].indices);
		memset(&meshes[mesh_count], 0, sizeof(meshes[mesh_count]));
		failed = resource_failed = 1;
		platform_log("GPU: modern shadow mesh upload failed; retaining existing lighting until reset");
		return 0;
	}
	meshes[mesh_count].count = index_count;
	mesh_bytes += size;
	return ++mesh_count;
}

int modern_shadow_draw(int mesh, const float *matrices, int node_count,
	const void *alpha_texture, const float uv_transform[8], int views)
{
	GLuint alpha = 0;
	GLenum target;
	struct xgpu_texture_description description;
	if (mesh < 1 || mesh > mesh_count || node_count < 1 || node_count > SHADOW_NODES ||
		draw_count == SHADOW_DRAWS || !drawing || views < 1 || views > MODERN_SHADOW_BOTH) { failed = 1; return 0; }
	if (alpha_texture)
	{
		glActiveTexture(GL_TEXTURE0 + 9);
		alpha = xgpu_texture_get(alpha_texture, NULL, &target, &description);
		if (!alpha || target != GL_TEXTURE_2D) { failed = 1; return 0; }
	}
	draws[draw_count].mesh = mesh - 1;
	draws[draw_count].nodes = node_count;
	draws[draw_count].views = views;
	draws[draw_count].alpha = alpha;
	memcpy(draws[draw_count].matrices, matrices, node_count * 16 * sizeof(float));
	memcpy(draws[draw_count].uv_transform, uv_transform, 8 * sizeof(float));
	draw_count++;
	return 1;
}

int modern_shadow_end(void)
{
	int face, draw, pass;
	if (!drawing) return 0;
	map->view_map = 0;
	for (draw = 0; draw < draw_count; draw++)
		if (draws[draw].views == MODERN_SHADOW_VIEW) map->view_map = 1;
	if (!failed && map->view_map && !map->depth_cube[1])
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		create_depth_cube(&map->depth_cube[1]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, map->depth_cube[1], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
		{
			failed = resource_failed = 1;
			platform_log("GPU: viewmodel shadow map unavailable; retaining existing lighting until reset");
		}
	}
	if (!failed)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
		glDepthRange(0, 1);
		glClearDepth(1);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glUseProgram(program);
		glUniform4fv(light_uniform, 1, map->light);
		glBindVertexArray(vao);
		glActiveTexture(GL_TEXTURE0 + 9);
		glBindSampler(9, alpha_sampler);
		for (pass = 0; pass < 1 + map->view_map; pass++)
		for (face = 0; face < 6; face++)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, map->depth_cube[pass], 0);
			glClear(GL_DEPTH_BUFFER_BIT);
			glUniform1i(face_uniform, face);
			for (draw = 0; draw < draw_count; draw++)
			{
				int mesh = draws[draw].mesh;
				if (!(draws[draw].views & (1 << pass))) continue;
				glBindBuffer(GL_ARRAY_BUFFER, meshes[mesh].vertices);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mesh].indices);
				glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
				glEnableVertexAttribArray(2); glEnableVertexAttribArray(3);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, position));
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, nodes));
				glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, weight));
				glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, uv));
				glUniformMatrix4fv(bones_uniform, draws[draw].nodes, GL_FALSE, draws[draw].matrices);
				glUniform4fv(uv_uniform, 2, draws[draw].uv_transform);
				glUniform1i(alpha_uniform, draws[draw].alpha != 0);
				glBindTexture(GL_TEXTURE_2D, draws[draw].alpha);
				glDrawElements(GL_TRIANGLES, meshes[mesh].count, GL_UNSIGNED_SHORT, NULL);
			}
		}
		map->valid = 1;
	}
	/* Match the renderer's external-pass handoff. Draws and clears bind
	   their own targets; invalidation reapplies the untouched D3D viewport,
	   depth range and render state. Only clip convention is context-global. */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
	xgpu_gl_end_external_pass();
	drawing = 0;
	return map->valid;
}

void modern_shadow_clear_draws(void)
{
	int i;
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++) maps[i].valid = 0;
	draw_count = 0;
}

void modern_shadow_dispose(void)
{
	int i;
	if (drawing) { failed = 1; modern_shadow_end(); }
	for (i = 0; i < mesh_count; i++)
	{
		glDeleteBuffers(1, &meshes[i].vertices);
		glDeleteBuffers(1, &meshes[i].indices);
	}
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
		if (maps[i].depth_cube[0] || maps[i].depth_cube[1]) glDeleteTextures(2, maps[i].depth_cube);
	memset(maps, 0, sizeof(maps));
	map = &maps[0];
	if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
	if (program) glDeleteProgram(program);
	if (vao) glDeleteVertexArrays(1, &vao);
	if (alpha_sampler) glDeleteSamplers(1, &alpha_sampler);
	mesh_count = mesh_bytes = draw_count = resource_failed = 0;
	framebuffer = program = vao = alpha_sampler = 0;
}

void modern_shadow_emit(struct xgpu_text *text)
{
	xgpu_text_append(text,
		"uniform samplerCube modern_depth, modern_depth_other; uniform vec4 modern_light, modern_light_other;\n"
		"uniform vec2 modern_slots; uniform int modern_soft; uniform float modern_extent;\n"
		/* Compare at the sampled texel's ray on the receiver plane. A fixed
		   depth offset either stripes grazing surfaces or detaches shadows. */
		"float modern_compare(samplerCube depth, float radius, vec3 sample_direction, vec3 receiver, vec3 normal) {\n"
		" vec3 a=abs(sample_direction); float major=max(a.x,max(a.y,a.z));\n"
		" vec3 d=sample_direction/major;\n"
		" vec3 q=(clamp(floor((d*0.5+0.5)*512.0),vec3(0),vec3(511))+0.5)/256.0-1.0;\n"
		" if(a.x>=a.y && a.x>=a.z) q.x=sign(d.x); else if(a.y>=a.z) q.y=sign(d.y); else q.z=sign(d.z);\n"
		" float distance=length(receiver); float denominator=dot(q,normal);\n"
		" float numerator=dot(receiver,normal);\n"
		/* Neighbouring rays at grazing incidence can meet the plane far
		   beyond the receiver. Clamping that intersection creates acne.
		   A ray facing away from the plane has no receiver to occlude. */
		" if(dot(normal,normal)>0.5 && numerator*denominator<=0.0) return 1.0;\n"
		" float plane=abs(denominator)>0.000001?numerator/denominator*length(q):distance;\n"
		/* Empty texels contain 1 even when the plane intersection is beyond
		   the light's range; they must not become artificial blockers. */
		" float z=min((plane-max(0.0005,plane*0.00001))/radius,1.0);\n"
		" return step(z,texture(depth,q).r);\n}\n"
		"float modern_visibility_map(samplerCube depth, vec4 light, vec3 d) {\n"
		/* Viewmodels and high-resolution receivers have very small screen
		   derivatives. Test only degeneracy, not world-space triangle size. */
		" vec3 n=cross(dFdx(d),dFdy(d)); float nl=length(n); n=nl>1.0e-20?n/nl:vec3(0);\n"
		" float distance=length(d); if(distance<0.01 || distance>=light.w) return 1.0;\n"
		" if(modern_soft==0) return modern_compare(depth,light.w,d,d,n);\n"
		/* Bilinear comparison weights move continuously across texels. The
		   four taps cross cube edges through the same ray/plane comparison;
		   no driver-dependent depth-compare filtering is required. */
		" vec3 a=abs(d); vec3 direction=d/max(a.x,max(a.y,a.z)); vec3 u,v; vec2 f;\n"
		" if(a.x>=a.y && a.x>=a.z) { u=vec3(0,1,0); v=vec3(0,0,1); f=direction.yz; }\n"
		" else if(a.y>=a.z) { u=vec3(1,0,0); v=vec3(0,0,1); f=direction.xz; }\n"
		" else { u=vec3(1,0,0); v=vec3(0,1,0); f=direction.xy; }\n"
		" f=fract(f*256.0+255.5); u/=256.0; v/=256.0; direction-=u*f.x+v*f.y;\n"
		" return mix(mix(modern_compare(depth,light.w,direction,d,n),modern_compare(depth,light.w,direction+u,d,n),f.x),\n"
		" mix(modern_compare(depth,light.w,direction+v,d,n),modern_compare(depth,light.w,direction+u+v,d,n),f.x),f.y);\n}\n"
		"float modern_visibility(vec3 d) { return modern_visibility_map(modern_depth,modern_light,d); }\n"
		"float modern_model(int slot,vec3 p) { if(modern_slots[slot]<0.5) return 1.0;\n"
		" return slot==0?modern_visibility(p-modern_light.xyz):modern_visibility_map(modern_depth_other,modern_light_other,p-modern_light_other.xyz); }\n");
}

void modern_shadow_uniforms_initialize(unsigned long linked, struct modern_shadow_uniforms *uniforms)
{
	uniforms->light = glGetUniformLocation(linked, "modern_light");
	uniforms->other_light = glGetUniformLocation(linked, "modern_light_other");
	uniforms->slots = glGetUniformLocation(linked, "modern_slots");
	uniforms->filter = glGetUniformLocation(linked, "modern_soft");
	uniforms->extent = glGetUniformLocation(linked, "modern_extent");
	if (uniforms->light >= 0) glUniform1i(glGetUniformLocation(linked, "modern_depth"), 8);
	if (uniforms->other_light >= 0) glUniform1i(glGetUniformLocation(linked, "modern_depth_other"), 10);
}

void modern_shadow_bind(const struct modern_shadow_uniforms *uniforms)
{
	float enabled[2] = {0,0};
	int slot;
	if (uniforms->light < 0 && uniforms->other_light < 0) return;
	for (slot = 0; slot < 2; slot++)
	{
		int index = modern_shadows_map(slot);
		struct shadow_map const *receiver;
		if (index < 0 || index >= MODERN_SHADOW_LIGHTS || !maps[index].valid) continue;
		receiver = &maps[index];
		enabled[slot] = 1;
		glBindTextures(slot ? 10 : 8, 1, &receiver->depth_cube[receiver->view_map && modern_shadows_view_receiver() ? 1 : 0]);
		glBindSampler(slot ? 10 : 8, 0);
		glUniform4fv(slot ? uniforms->other_light : uniforms->light, 1, receiver->light);
	}
	glUniform2f(uniforms->slots, enabled[0], enabled[1]);
	glUniform1i(uniforms->filter, soft_filter);
	glUniform1f(uniforms->extent, modern_shadows_world_extent());
}

#else
int modern_shadow_mesh_create(const struct modern_shadow_vertex *v, int nv, const unsigned short *i, int ni) { return 0; }
int modern_shadow_draw(int mesh, const float *m, int n, const void *a, const float uv[8], int views) { return 0; }
int modern_shadow_begin(int slot, const float p[3], float r, int soft) { return 0; }
int modern_shadow_end(void) { return 0; }
void modern_shadow_clear_draws(void) {}
void modern_shadow_dispose(void) {}
void modern_shadow_emit(struct xgpu_text *text) {}
void modern_shadow_uniforms_initialize(unsigned long p, struct modern_shadow_uniforms *u) { u->light = -1; }
void modern_shadow_bind(const struct modern_shadow_uniforms *u) {}
#endif
