/* Feature-owned point-light depth maps. No stock render target or shader is
   modified. GLES has no shadow resources or shader variants. */
#include "modern_shadows.h"
#include "xgpu.h"
#include <stddef.h>
#include <string.h>

#ifndef HALO_GLES
enum { SHADOW_MESHES = 2048, SHADOW_DRAWS = 256,
	SHADOW_BYTES = 32 * 1024 * 1024, SHADOW_NODES = 64 };
static struct shadow_mesh
{
	GLuint vertices, indices, array;
	int count, bounded, model;
	int handoff_nodes;
	GLuint handoff_alpha;
	unsigned int nodes[2];
	float minimum[3], maximum[3];
} meshes[SHADOW_MESHES];
static struct
{
	int mesh, nodes, views, faces, sun_detail;
	unsigned long opacity_serial;
	float matrices[SHADOW_NODES * 16], uv_transform[8];
	GLuint alpha;
} draws[SHADOW_DRAWS];
static int mesh_count, draw_count, mesh_bytes, failed, soft_filter, resource_failed;
static struct shadow_map
{
	GLuint depth_cube[2];
	float light[4];
	int valid, view_map;
	/* Only confirmed clear faces survive between uses of this cube. */
	unsigned int clear_faces, view_clear_faces;
} maps[MODERN_SHADOW_LIGHTS];
static struct shadow_map *map = &maps[0];
static GLuint framebuffer, program, vao, alpha_sampler;
static GLint light_uniform, face_uniform, bones_uniform, alpha_uniform, uv_uniform;
static GLuint opaque_program;
static GLint opaque_light, opaque_face, opaque_bones;
static int drawing;
static int environment_drawing;
static int shadow_size;
static float ground_slope;
static int correct_ground_depth;
extern long halo_shadow_resolution_scale(void);

/* A blend of two transformed vertices stays in the union's bounding box.
   Retaining all used bones makes this conservative for animated models too. */
static void mesh_bounds(struct shadow_mesh *mesh, const struct modern_shadow_vertex *vertices, int count)
{
	int i, axis, node;
	mesh->nodes[0] = mesh->nodes[1] = 0;
	mesh->bounded = 0;
	for (i = 0; i < count; i++)
	{
		const struct modern_shadow_vertex *v = &vertices[i];
		if (!(v->weight >= 0 && v->weight <= 1)) return;
		for (axis = 0; axis < 3; axis++)
		{
			float p = v->position[axis];
			if (!(p > -1.e30f && p < 1.e30f)) return;
			if (!i || p < mesh->minimum[axis]) mesh->minimum[axis] = p;
			if (!i || p > mesh->maximum[axis]) mesh->maximum[axis] = p;
		}
		for (axis = 0; axis < 2; axis++)
		{
			if (!(v->nodes[axis] >= 0 && v->nodes[axis] < SHADOW_NODES)) return;
			node = (int)v->nodes[axis];
			if ((axis ? 1.f - v->weight : v->weight) > 0)
				mesh->nodes[node / 32] |= 1U << (node % 32);
		}
	}
	mesh->bounded = count > 0;
}

/* Reject only when the complete posed box misses the light or a face.
   Unknown bounds retain the original six-face submission. The margin
   covers CPU/GPU rounding at clip seams; it never moves rendered vertices. */
static int mesh_faces(const struct shadow_mesh *mesh, const float *matrices, int count, const float light[4])
{
	float minimum[3] = {0}, maximum[3] = {0}, distance = 0;
	int node, axis, j, first = 1, face, faces = 0;
	if (!mesh->bounded) return 63;
	for (node = 0; node < SHADOW_NODES; node++)
	{
		const float *m;
		if (!(mesh->nodes[node / 32] & (1U << (node % 32)))) continue;
		if (node >= count) return 63;
		m = matrices + node * 16;
		if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1) return 63;
		for (axis = 0; axis < 3; axis++)
		{
			float low = m[12 + axis], high = low, margin;
			for (j = 0; j < 3; j++)
			{
				float a = m[j * 4 + axis] * mesh->minimum[j];
				float b = m[j * 4 + axis] * mesh->maximum[j];
				low += a < b ? a : b;
				high += a > b ? a : b;
			}
			if (!(low > -1.e30f && high < 1.e30f)) return 63;
			margin = .0001f * (1.f + (low < 0 ? -low : low) + (high < 0 ? -high : high));
			low -= light[axis] + margin;
			high -= light[axis] - margin;
			if (first || low < minimum[axis]) minimum[axis] = low;
			if (first || high > maximum[axis]) maximum[axis] = high;
		}
		first = 0;
	}
	if (first) return 63;
	for (axis = 0; axis < 3; axis++)
	{
		float d = minimum[axis] > 0 ? minimum[axis] : maximum[axis] < 0 ? -maximum[axis] : 0;
		distance += d * d;
	}
	if (distance > light[3] * light[3]) return 0;
	for (face = 0; face < 6; face++)
	{
		float depth;
		axis = face / 2;
		depth = face & 1 ? -minimum[axis] : maximum[axis];
		if (depth < .00001f) continue;
		for (j = 0; j < 3; j++)
			if (j != axis && (depth + maximum[j] < 0 || depth - minimum[j] < 0)) break;
		if (j == 3) faces |= 1 << face;
	}
	return faces;
}

static void release_maps(void)
{
	int i;
	for (i = 0; i < MODERN_SHADOW_LIGHTS; i++)
		if (maps[i].depth_cube[0] || maps[i].depth_cube[1]) glDeleteTextures(2, maps[i].depth_cube);
	memset(maps, 0, sizeof(maps));
}

/* The shared setting is committed between frames. Keep immutable meshes
   and programs, but release every old-resolution cube before allocating
   any new one. A setting change can retry a failed allocation. */
static void resize_maps(int size)
{
	if (shadow_size == size) return;
	release_maps();
	shadow_size = size;
	resource_failed = 0;
}

/* Each immutable mesh owns its vertex layout. Adjacent parts often share the
   same pose; avoid uploading identical bones or cutout state across draws. */
static void bind_caster_draw(int draw, int *previous, GLint bones, GLint uv, GLint alpha)
{
	int last = *previous;
	if (last < 0 || draws[last].mesh != draws[draw].mesh)
		glBindVertexArray(meshes[draws[draw].mesh].array);
	if (last < 0 || draws[last].nodes != draws[draw].nodes ||
		memcmp(draws[last].matrices, draws[draw].matrices, draws[draw].nodes * 16 * sizeof(float)))
		glUniformMatrix4fv(bones, draws[draw].nodes, GL_FALSE, draws[draw].matrices);
	if (alpha >= 0 && (last < 0 || !draws[last].alpha != !draws[draw].alpha))
		glUniform1i(alpha, draws[draw].alpha != 0);
	if (alpha >= 0 && draws[draw].alpha)
	{
		if (last < 0 || draws[last].alpha != draws[draw].alpha)
			glBindTexture(GL_TEXTURE_2D, draws[draw].alpha);
		if (last < 0 || !draws[last].alpha ||
			memcmp(draws[last].uv_transform, draws[draw].uv_transform, sizeof(draws[draw].uv_transform)))
			glUniform4fv(uv, 2, draws[draw].uv_transform);
	}
	*previous = draw;
}

static void create_depth_cube(GLuint *texture)
{
	int face;
	glGenTextures(1, texture);
	glActiveTexture(GL_TEXTURE0 + 8);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *texture);
	for (face = 0; face < 6; face++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT32F,
			shadow_size, shadow_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

static int create_resources(void)
{
	/* Keep the perspective near plane below the receiver tolerance. A 0.005
	   plane clipped valid close casters (stuck projectiles and held geometry).
	   Reversed 32-bit floating depth retains relative precision at distance;
	   mesh_faces and the receiver decoder use the same 0.00001 value. */
	static const char vertex[] =
		"#version 330 core\n"
		"layout(location=0) in vec3 position;\n"
		"layout(location=1) in vec2 nodes;\n"
		"layout(location=2) in float weight;\n"
		"layout(location=3) in vec2 uv;\n"
		"uniform mat4 bones[64]; uniform vec4 light; uniform int face;\n"
		"uniform vec4 uv_transform[2]; out vec2 texcoord;\n"
		"void main() {\n"
		" vec4 p = mix(bones[int(nodes.y)]*vec4(position,1), bones[int(nodes.x)]*vec4(position,1),weight);\n"
		" texcoord=vec2(dot(vec4(uv,0,1),uv_transform[0]),dot(vec4(uv,0,1),uv_transform[1]));\n"
		" vec3 d=p.xyz-light.xyz; vec3 v;\n"
		" if(face==0) v=vec3(-d.z,-d.y,d.x);\n"
		" else if(face==1) v=vec3(d.z,-d.y,-d.x);\n"
		" else if(face==2) v=vec3(d.x,d.z,d.y);\n"
		" else if(face==3) v=vec3(d.x,-d.z,-d.y);\n"
		" else if(face==4) v=vec3(d.x,-d.y,d.z);\n"
		" else v=vec3(-d.x,-d.y,-d.z);\n"
		" float n=0.00001; float f=light.w;\n"
		" gl_Position=vec4(v.xy,n*(f-v.z)/(f-n),v.z);\n}\n";
	static const char fragment[] =
		"#version 330 core\n"
		"in vec2 texcoord;\n"
		"uniform sampler2D alpha_map; uniform int alpha_test;\n"
		"void main() {\n"
		" if(alpha_test!=0 && texture(alpha_map,texcoord).a <= (127.0/255.0)) discard;\n"
		"}\n";
	GLuint vs, fs;
	GLint subpixel_bits = 0;
	GLenum none = GL_NONE;
	if (program) return 1;
	vs = xgpu_compile_shader(GL_VERTEX_SHADER, vertex, "modern shadow caster");
	fs = xgpu_compile_shader(GL_FRAGMENT_SHADER, fragment, "modern shadow depth");
	program = vs && fs ? xgpu_link_program(vs, fs, "modern shadow depth") : 0;
	if (program)
	{
		GLuint depth = xgpu_compile_shader(GL_FRAGMENT_SHADER, "#version 330 core\nvoid main(){}",
			"modern opaque shadow depth");
		if (depth)
		{
			opaque_program = xgpu_link_program(vs, depth, "modern opaque shadow depth");
			glDeleteShader(depth);
		}
		if (opaque_program)
		{
			opaque_light = glGetUniformLocation(opaque_program, "light");
			opaque_face = glGetUniformLocation(opaque_program, "face");
			opaque_bones = glGetUniformLocation(opaque_program, "bones");
		}
	}
	if (vs) glDeleteShader(vs);
	if (fs) glDeleteShader(fs);
	if (!program) return 0;
	/* Half a raster subpixel of slope compensates for steep depth gradients
	   under a settled grenade. Negative moves casters away in reversed Z. */
	glGetIntegerv(GL_SUBPIXEL_BITS, &subpixel_bits);
	ground_slope = subpixel_bits > 0 && subpixel_bits < 21 ? -0.5f / (1u << subpixel_bits) : 0.f;
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

/* Keep the same lazy and load-time validation for a complete depth cube. */
static int prepare_depth_cube(GLuint *cube)
{
	GLenum status, error;
	if (*cube) return 1;
	create_depth_cube(cube);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, *cube, 0);
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	error = glGetError();
	if (status == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR)
		return 1;
	glDeleteTextures(1, cube);
	*cube = 0;
	return 0;
}

int modern_shadow_begin(int slot, const float position[3], float radius, int soft, int ground_grenade)
{
	if (slot < 0 || slot >= MODERN_SHADOW_LIGHTS || drawing) return 0;
	if (!xgpu_gl_ready() || radius <= 0.01f) return 0;
	resize_maps(128 * (int)halo_shadow_resolution_scale());
	map = &maps[slot];
	map->valid = 0;
	draw_count = 0;
	if (resource_failed) return 0;
	drawing = 1;
	failed = !create_resources() || !prepare_depth_cube(&map->depth_cube[0]);
	memcpy(map->light, position, 3 * sizeof(float));
	map->light[3] = radius;
	soft_filter = soft;
	correct_ground_depth = ground_grenade && !soft && shadow_size == 1024 && ground_slope != 0.f;
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
	const unsigned short *indices, int index_count, int model)
{
	int size, index;
	if (!drawing || vertex_count < 1 || vertex_count > 65536 || index_count < 1 || index_count > 196608 ||
		mesh_count == SHADOW_MESHES) return 0;
	size = vertex_count * sizeof(*vertices) + index_count * sizeof(*indices);
	if (size > SHADOW_BYTES - mesh_bytes) return 0;
	for (index = 0; index < index_count; index++) if (indices[index] >= vertex_count) return 0;
	meshes[mesh_count].model = model;
	meshes[mesh_count].handoff_nodes = 0;
	mesh_bounds(&meshes[mesh_count], vertices, vertex_count);
	glGenVertexArrays(1, &meshes[mesh_count].array);
	glBindVertexArray(meshes[mesh_count].array);
	glGenBuffers(1, &meshes[mesh_count].vertices);
	glBindBuffer(GL_ARRAY_BUFFER, meshes[mesh_count].vertices);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(*vertices), vertices, GL_STATIC_DRAW);
	glGenBuffers(1, &meshes[mesh_count].indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mesh_count].indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(*indices), indices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2); glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, nodes));
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, weight));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(struct modern_shadow_vertex), (void *)offsetof(struct modern_shadow_vertex, uv));
	if (glGetError() != GL_NO_ERROR)
	{
		glDeleteVertexArrays(1, &meshes[mesh_count].array);
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

static int sun_detail_caster, sun_detail_in_batch;
void modern_environment_sun_detail_caster(int eligible) { sun_detail_caster = eligible; }
static int sun_draw_batch(void);
static int flush_beam_batch(void);

int modern_shadow_draw(int mesh, const float *matrices, int node_count,
	const void *alpha_texture, const float uv_transform[8], int views)
{
	GLuint alpha = 0;
	GLenum target;
	struct xgpu_texture_description description;
	int faces, opaque;
	if (draw_count == SHADOW_DRAWS && environment_drawing && !flush_beam_batch()) sun_draw_batch();
	if (mesh < 1 || mesh > mesh_count || node_count < 1 || node_count > SHADOW_NODES ||
		draw_count == SHADOW_DRAWS || !drawing || views < 1 || views > MODERN_SHADOW_BOTH) { failed = 1; return 0; }
	faces = environment_drawing ? 63 : mesh_faces(&meshes[mesh - 1], matrices, node_count, map->light);
	if (!faces) return 1;
	if (alpha_texture)
	{
		glActiveTexture(GL_TEXTURE0 + 9);
		alpha = xgpu_texture_get(alpha_texture, NULL, &target, &description);
		if (!alpha || target != GL_TEXTURE_2D) { failed = 1; return 0; }
	}
	/* World geometry without a cutout has no texture to sample. Models
	   qualify through the uploaded base texture's opacity classification. */
	opaque = meshes[mesh - 1].model ? alpha && description.base_opaque : !alpha_texture;
	draws[draw_count].opacity_serial = opaque && !environment_drawing && opaque_program ? xgpu_texture_lookup_serial() : 0;
	draws[draw_count].sun_detail = sun_detail_caster;
	sun_detail_in_batch |= sun_detail_caster;
	draws[draw_count].mesh = mesh - 1;
	draws[draw_count].nodes = node_count;
	draws[draw_count].views = views;
	draws[draw_count].faces = faces;
	draws[draw_count].alpha = alpha;
	memcpy(draws[draw_count].matrices, matrices, node_count * 16 * sizeof(float));
	memcpy(draws[draw_count].uv_transform, uv_transform, 8 * sizeof(float));
	draw_count++;
	return 1;
}

int modern_shadow_end(void)
{
	int previous = -1, using_opaque = 0, opaque_light_set = 0;
	unsigned long serial = xgpu_texture_lookup_serial();
	int face, draw, pass, shared, common_faces = 0, unique_faces[2] = {0, 0};
	if (!drawing) return 0;
	map->view_map = 0;
	for (draw = 0; draw < draw_count; draw++)
	{
		if (draws[draw].views == MODERN_SHADOW_VIEW) map->view_map = 1;
		if (draws[draw].views != MODERN_SHADOW_BOTH)
			unique_faces[draws[draw].views - 1] |= draws[draw].faces;
		else common_faces |= draws[draw].faces;
	}
	shared = map->view_map;
	if (!failed && map->view_map && !prepare_depth_cube(&map->depth_cube[1]))
	{
		failed = resource_failed = 1;
		platform_log("GPU: viewmodel shadow map unavailable; retaining existing lighting until reset");
	}
	if (!failed)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(0, 0, shadow_size, shadow_size);
		/* Reversed floating-point depth keeps distant precision and permits
		   early depth rejection without a fragment-depth override. */
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		glDepthRange(0, 1);
		glClearDepth(0);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_GREATER);
		glDepthMask(GL_TRUE);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		if (correct_ground_depth)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(ground_slope, 0.f);
		}
		else glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glUseProgram(program);
		glUniform4fv(light_uniform, 1, map->light);
		glBindVertexArray(vao);
		glActiveTexture(GL_TEXTURE0 + 9);
		glBindSampler(9, alpha_sampler);
		/* World and viewmodel receivers share the same external geometry.
		   Render that depth once, copy it exactly, then add only each view's
		   private casters. The hidden body never enters the viewmodel cube. */
		for (pass = 0; pass < (shared ? 3 : 1 + map->view_map); pass++)
		{
			if (shared && pass == 1)
			{
				/* Empty on both sides already has the right depth. Keep a single
				   contiguous copy, including any empty faces between its ends. */
				unsigned int copy = common_faces | (63 & ~map->view_clear_faces);
				if (copy)
				{
					int first = 0, last = 5;
					while (!(copy & (1U << first))) first++;
					while (!(copy & (1U << last))) last--;
					glCopyImageSubData(map->depth_cube[0], GL_TEXTURE_CUBE_MAP, 0, 0, 0, first,
						map->depth_cube[1], GL_TEXTURE_CUBE_MAP, 0, 0, 0, first, shadow_size, shadow_size, last-first+1);
				}
			}
			for (face = 0; face < 6; face++)
			{
				int bit = 1 << face;
				if (shared && pass && !(unique_faces[pass - 1] & bit)) continue;
				if (!pass && (map->clear_faces & bit) &&
					!((common_faces | (shared ? 0 : unique_faces[0])) & bit)) continue;
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
					map->depth_cube[shared ? pass == 2 : pass], 0);
				if ((!pass && !(map->clear_faces & bit)) || (pass && !shared)) glClear(GL_DEPTH_BUFFER_BIT);
				glUniform1i(using_opaque ? opaque_face : face_uniform, face);
				for (draw = 0; draw < draw_count; draw++)
				{
					int mesh = draws[draw].mesh;
					int opaque = draws[draw].opacity_serial && draws[draw].opacity_serial == serial;
					if (shared ? draws[draw].views != (pass ? pass : MODERN_SHADOW_BOTH) :
						!(draws[draw].views & (1 << pass))) continue;
					if (!(draws[draw].faces & (1 << face))) continue;
					if (opaque != using_opaque)
					{
						using_opaque = opaque;
						previous = -1;
						glUseProgram(opaque ? opaque_program : program);
						glUniform1i(opaque ? opaque_face : face_uniform, face);
						if (opaque && !opaque_light_set)
						{
							glUniform4fv(opaque_light, 1, map->light);
							opaque_light_set = 1;
						}
					}
					if (opaque) bind_caster_draw(draw, &previous, opaque_bones, -1, -1);
					else bind_caster_draw(draw, &previous, bones_uniform, uv_uniform, alpha_uniform);
					glDrawElements(GL_TRIANGLES, meshes[mesh].count, GL_UNSIGNED_SHORT, NULL);
				}
			}
		}
		if (correct_ground_depth) glDisable(GL_POLYGON_OFFSET_FILL);
		map->clear_faces = 63 & ~(common_faces | unique_faces[0]);
		if (map->view_map) map->view_clear_faces = 63 & ~(common_faces | unique_faces[1]);
		map->valid = 1;
	}
	/* Do not retain a partially allocated high-resolution set after a
	   resource failure. Receivers fall back until reset or a size change. */
	if (resource_failed) release_maps();
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

#include "modern_environment_gl.inl"
#include "modern_spots_gl.inl"

void modern_shadow_preload(int sunlight, int fixtures, int beam)
{
	GLint read_fbo, draw_fbo;
	if (!xgpu_gl_ready()) return;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
	resize_maps(128 * (int)halo_shadow_resolution_scale());
	/* A full set at 1024 consumes 192 MiB. Keep larger cubes and
	   first-person copies demand-allocated so sparse maps can still run. */
	if (shadow_size <= 1024 && create_resources())
	{
		int slot;
		for (slot = 0; slot < MODERN_SHADOW_LIGHTS; slot++)
			if (!prepare_depth_cube(&maps[slot].depth_cube[0])) break;
	}
	sun.projecting = sunlight;
	if (environment_resources_for(0))
	{
		if (shadow_size <= 1024) environment_model_resources();
		if (sunlight) sun_depth_resources();
		if (fixtures || beam) spot_depth_resources(shadow_size, fixtures ? SPOT_LAYERS : 1);
		if (fixtures) spot_receiver_prepare();
	}
	sun.projecting = 0;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
	xgpu_gl_end_external_pass();
}

static void dispose_shadow_meshes(void)
{
	int i;
	for (i = 0; i < mesh_count; i++)
	{
		glDeleteBuffers(1, &meshes[i].vertices);
		glDeleteBuffers(1, &meshes[i].indices);
		glDeleteVertexArrays(1, &meshes[i].array);
	}
	mesh_count = mesh_bytes = draw_count = 0;
}

void modern_environment_invalidate_static(void)
{
	sun.valid = spots.valid_static = 0;
}

void modern_shadow_change_bsp(void)
{
	/* This hook runs between windows. The images may be reused, but no
	   visibility, cutout pointers or immutable mesh belongs to the next BSP. */
	dispose_shadow_meshes();
	modern_shadow_clear_draws();
	modern_environment_window();
	memset(sun.surface_sky, 0, sizeof(sun.surface_sky));
	modern_environment_invalidate_static();
	environment_model.unavailable = environment.failed_size = resource_failed = 0;
}

void modern_shadow_dispose(void)
{
	modern_environment_dispose();
	if (drawing) { failed = 1; modern_shadow_end(); }
	dispose_shadow_meshes();
	release_maps();
	map = &maps[0];
	if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
	if (opaque_program) glDeleteProgram(opaque_program);
	if (program) glDeleteProgram(program);
	if (vao) glDeleteVertexArrays(1, &vao);
	if (alpha_sampler) glDeleteSamplers(1, &alpha_sampler);
	resource_failed = 0;
	framebuffer = program = opaque_program = vao = alpha_sampler = 0;
}

void modern_shadow_emit(struct xgpu_text *text)
{
	xgpu_text_append(text,shadow_filter);
	xgpu_text_append(text,sun_coverage);
	xgpu_text_append(text,sun_lighting);
	xgpu_text_append(text,
		"uniform sampler2D modern_environment_depth0, modern_environment_depth1;\n"
		"uniform mat4 modern_environment_frames[2];\n"
		"uniform float modern_environment_radius;\n"
		"uniform int modern_environment_mask, modern_environment_soft;\n"
		"float modern_environment_compare(sampler2D map, vec2 uv, float depth, vec2 gradient) {\n"
		" ivec2 size=textureSize(map,0), coord=ivec2(floor(uv*vec2(size)));\n"
		" if(any(lessThan(coord,ivec2(0)))||any(greaterThanEqual(coord,size))) return 1.0;\n"
		" vec2 center=(vec2(coord)+0.5)/vec2(size); float stored=texelFetch(map,coord,0).r;\n"
		" return stored>0 && stored>depth+dot(center-uv,gradient)+0.0001?0.0:1.0;\n"
		"}\n"
		"float modern_environment_visibility(int slot, vec3 position) {\n"
		" if((modern_environment_mask & (1<<slot))==0) return 1.0;\n"
		" mat4 frame=modern_environment_frames[slot];\n"
		" vec3 q=transpose(mat3(frame))*(position-frame[3].xyz)/modern_environment_radius;\n"
		" if(any(greaterThan(abs(q),vec3(1)))) return 1.0;\n"
		" vec2 uv=q.xy*0.5+0.5; float depth=0.5-0.5*q.z;\n"
		" vec3 dx=dFdx(vec3(uv,depth)),dy=dFdy(vec3(uv,depth)); float det=dx.x*dy.y-dx.y*dy.x;\n"
		" vec2 gradient=abs(det)>1.e-12?vec2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/det:vec2(0);\n"
		" vec2 size=vec2(textureSize(modern_environment_depth0,0)), f=vec2(0), base=uv;\n"
		" if(modern_environment_soft!=0) { f=fract(uv*size-0.5); base-=f/size; }\n"
		" float result=0.0;\n"
		" for(int tap=0;tap<=3*modern_environment_soft;tap++) {\n"
		"  int x=tap&1, y=tap>>1; vec2 sample_uv=base+vec2(x,y)/size;\n"
		"  float z=depth+dot(sample_uv-uv,gradient);\n"
		"  float v=slot==0?modern_environment_compare(modern_environment_depth0,sample_uv,z,gradient):modern_environment_compare(modern_environment_depth1,sample_uv,z,gradient);\n"
		"  result+=v*(x==0?1-f.x:f.x)*(y==0?1-f.y:f.y);\n"
		" }\n"
		" return result;\n"
		"}\n"
	);

	xgpu_text_append(text,
		"uniform samplerCube modern_depth, modern_depth_other; uniform vec4 modern_light, modern_light_other;\n"
		"uniform vec2 modern_slots; uniform int modern_soft; uniform float modern_extent;\n"
		/* Compare at the sampled texel's ray on the receiver plane. A fixed
		   depth offset either stripes grazing surfaces or detaches shadows. */
		"float modern_compare(samplerCube depth, float radius, vec3 sample_direction, vec3 receiver, vec3 normal) {\n"
		" vec3 a=abs(sample_direction); float major=max(a.x,max(a.y,a.z));\n"
		" vec3 d=sample_direction/major;\n"
		" float size=float(textureSize(depth,0).x);\n"
		" vec3 q=(clamp(floor((d*0.5+0.5)*size),vec3(0),vec3(size-1.0))+0.5)*(2.0/size)-1.0;\n"
		" bool x=a.x==major, y=!x && a.y==major; q=mix(q,sign(d),bvec3(x,y,!x && !y));\n"
		" float distance=length(receiver); float denominator=dot(q,normal);\n"
		" float numerator=dot(receiver,normal);\n"
		/* Neighbouring rays at grazing incidence can meet the plane far
		   beyond the receiver. Clamping that intersection creates acne.
		   A ray facing away from the plane has no receiver to occlude. */
		" if(dot(normal,normal)>0.5 && numerator*denominator<=0.0) return 1.0;\n"
		" float plane=abs(denominator)>0.000001?numerator/denominator*length(q):distance;\n"
		/* An empty texel decodes to 1 even when the plane intersection is
		   beyond the light's range; it must not become an artificial blocker. */
		" float z=min((plane-max(0.0005,plane*0.00001))/radius,1.0);\n"
		/* Recover radial distance / radius from hardware perspective depth.
		   Clear depth zero remains an empty ray (distance at least radius). */
		" float stored=min(0.00001*length(q)/(0.00001+textureLod(depth,q,0.0).r*(radius-0.00001)),1.0);\n"
		" return shadow_visible(z,stored);\n}\n"
		"float modern_visibility_map(samplerCube depth, vec4 light, vec3 d, bool contributes, bool model) {\n"
		/* Viewmodels and high-resolution receivers have very small screen
		   derivatives. Test only degeneracy, not world-space triangle size.
		   Evaluate them before skipping a zero-attenuation world fragment. */
		" vec3 n=cross(dFdx(d),dFdy(d)); float nl=length(n); n=nl>1.0e-20?n/nl:vec3(0);\n"
		" float distance=length(d); if(!contributes || distance<0.00001 || distance>=light.w) return 1.0;\n"
		/* One comparison for Hard, four bilinear comparisons for Soft. Keep
		   the tap count uniform so model programs share the receiver math. */
		" if(!model && modern_soft==0) return modern_compare(depth,light.w,d,d,n);\n"
		" vec3 direction=d, u=vec3(0), v=vec3(0); vec2 f=vec2(0);\n"
		" if(modern_soft!=0) {\n"
		"  vec3 a=abs(d); direction=d/max(a.x,max(a.y,a.z));\n"
		"  if(a.x>=a.y && a.x>=a.z) { u.y=1; v.z=1; f=direction.yz; }\n"
		"  else if(a.y>=a.z) { u.x=1; v.z=1; f=direction.xz; }\n"
		"  else { u.x=1; v.y=1; f=direction.xy; }\n"
		"  float half_size=float(textureSize(depth,0).x)*0.5;\n"
		"  f=fract(f*half_size+half_size-0.5); u/=half_size; v/=half_size; direction-=u*f.x+v*f.y;\n"
		" }\n"
		/* World lighting covers the screen once per light. Keep its four
		   samples independent so the GPU can issue their lookups together. */
		" if(!model) return shadow_filter4(vec4(modern_compare(depth,light.w,direction,d,n),modern_compare(depth,light.w,direction+u,d,n),\n"
		"  modern_compare(depth,light.w,direction+v,d,n),modern_compare(depth,light.w,direction+u+v,d,n)),f);\n"
		" vec4 samples=vec4(0);\n"
		" for(int tap=0;tap<=3*modern_soft;tap++) {\n"
		"  vec3 ray=direction; if((tap&1)!=0) ray+=u; if(tap>=2) ray+=v;\n"
		"  samples[tap]=modern_compare(depth,light.w,ray,d,n);\n"
		" }\n"
		" return shadow_filter4(samples,f);\n}\n"
		"float modern_visibility(vec3 d) { return modern_visibility_map(modern_depth,modern_light,d,true,true); }\n"
		"float modern_world(vec3 d,vec4 attenuation) { return modern_visibility_map(modern_depth,modern_light,d,any(notEqual(attenuation,vec4(0))),false); }\n"
		"float modern_model(int slot,vec3 p) { if(modern_slots[slot]<0.5) return 1.0;\n"
		" return slot==0?modern_visibility(p-modern_light.xyz):modern_visibility_map(modern_depth_other,modern_light_other,p-modern_light_other.xyz,true,true); }\n");
}

void modern_shadow_uniforms_initialize(unsigned long linked, struct modern_shadow_uniforms *uniforms)
{
    spot_locations(linked,uniforms);
    uniforms->last_slots = uniforms->last_filter = uniforms->last_environment_mask = -1;
    uniforms->sun_window = 0;
    uniforms->sun_enabled = 0;
    uniforms->sun_frame = glGetUniformLocation(linked,"sun_frame");
    uniforms->sun_color_radius = glGetUniformLocation(linked,"sun_color_radius");
    uniforms->sun_soft = glGetUniformLocation(linked,"sun_soft");
    uniforms->sun_close = glGetUniformLocation(linked,"sun_close");
    uniforms->sun_eye = glGetUniformLocation(linked,"sun_eye");
    uniforms->sun_detail = glGetUniformLocation(linked,"sun_detail");
    uniforms->sun_region = glGetUniformLocation(linked,"sun_region[0]");
    glUniform1i(glGetUniformLocation(linked,"sun_bsp"),14);
    glUniform1i(glGetUniformLocation(linked,"sun_objects"),15);
    uniforms->environment_frames = glGetUniformLocation(linked, "modern_environment_frames");
    uniforms->environment_radius = glGetUniformLocation(linked, "modern_environment_radius");
    uniforms->environment_mask = glGetUniformLocation(linked, "modern_environment_mask");
    uniforms->environment_filter = glGetUniformLocation(linked, "modern_environment_soft");
    glUniform1i(glGetUniformLocation(linked, "modern_environment_depth0"), 11);
    glUniform1i(glGetUniformLocation(linked, "modern_environment_depth1"), 12);
	uniforms->light = glGetUniformLocation(linked, "modern_light");
	uniforms->other_light = glGetUniformLocation(linked, "modern_light_other");
	uniforms->slots = glGetUniformLocation(linked, "modern_slots");
	uniforms->filter = glGetUniformLocation(linked, "modern_soft");
	uniforms->extent = glGetUniformLocation(linked, "modern_extent");
	if (uniforms->light >= 0) glUniform1i(glGetUniformLocation(linked, "modern_depth"), 8);
	if (uniforms->other_light >= 0) glUniform1i(glGetUniformLocation(linked, "modern_depth_other"), 10);
}

void modern_shadow_bind(struct modern_shadow_uniforms *uniforms)
{
	int enabled = 0;
	int slot;
    spot_bind(uniforms);
    if (uniforms->sun_color_radius >= 0 &&
        (uniforms->sun_enabled != sun.model || (sun.model && uniforms->sun_window != spot_window)))
    {
        uniforms->sun_enabled = sun.model;
        if (sun.model && uniforms->sun_window != spot_window)
        {
            uniforms->sun_window = spot_window;
            glUniformMatrix4fv(uniforms->sun_frame,1,GL_FALSE,sun.frame);
            glUniform1i(uniforms->sun_soft,sun.soft);
            glUniform4fv(uniforms->sun_close,1,sun.close);
            glUniform4fv(uniforms->sun_eye,1,sun.eye);
            glUniform4fv(uniforms->sun_detail,1,sun.detail);
            glUniform4fv(uniforms->sun_region,2,sun.region);
        }
        glUniform4fv(uniforms->sun_color_radius,1,(const float[4]){sun.tint[0],sun.tint[1],sun.tint[2],sun.model ? sun.radius : 0});
    }
    if (uniforms->last_environment_mask != environment_model.enabled)
    {
        glUniform1i(uniforms->environment_mask, environment_model.enabled);
        uniforms->last_environment_mask = environment_model.enabled;
    }
    if (environment_model.enabled)
    {
        glBindTextures(11, 2, environment_model.depth);
        glBindSampler(11, 0); glBindSampler(12, 0);
        glUniformMatrix4fv(uniforms->environment_frames, 2, GL_FALSE, environment_model.frames);
        glUniform1f(uniforms->environment_radius, environment_model.radius);
        glUniform1i(uniforms->environment_filter, environment_model.soft);
    }
	if (uniforms->light < 0 && uniforms->other_light < 0) return;
	for (slot = 0; slot < 2; slot++)
	{
		int index = modern_shadows_map(slot);
		struct shadow_map const *receiver;
		if (index < 0 || index >= MODERN_SHADOW_LIGHTS || !maps[index].valid) continue;
		receiver = &maps[index];
		enabled |= 1 << slot;
		glBindTextures(slot ? 10 : 8, 1, &receiver->depth_cube[receiver->view_map && modern_shadows_view_receiver() ? 1 : 0]);
		glBindSampler(slot ? 10 : 8, 0);
		glUniform4fv(slot ? uniforms->other_light : uniforms->light, 1, receiver->light);
	}
	/* Uniform values belong to each linked program. Texture bindings above
	   still follow the receiver, including first-person and external draws. */
	if (uniforms->last_slots != enabled)
	{
		glUniform2f(uniforms->slots, (enabled & 1) != 0, (enabled & 2) != 0);
		uniforms->last_slots = enabled;
	}
	if (uniforms->last_filter != soft_filter)
	{
		glUniform1i(uniforms->filter, soft_filter);
		uniforms->last_filter = soft_filter;
	}
	glUniform1f(uniforms->extent, modern_shadows_world_extent());
}

#else
void modern_shadow_preload(int sunlight, int fixtures, int beam) { (void)sunlight; (void)fixtures; (void)beam; }
int modern_spot_mask(void) { return 0; }
void modern_environment_model_clear(void) {}
int modern_environment_model_active(void) { return 0; }
int modern_environment_model_reuse(void) { return 0; }
int modern_environment_model_begin(const float f[32], float r, int s) { return 0; }
void modern_environment_model_end(int c, int m) {}
void modern_environment_receivers(int enabled) {}
int modern_environment_marking(void) { return 0; }
void modern_environment_window(void) {}
void modern_environment_dispose(void) {}
int modern_environment_begin(const float f[16], float r, int s) { return 0; }
int modern_environment_end(int c, const float v[16], const float p[16], const float rgb[3], int handoff) { return 0; }
int modern_shadow_mesh_create(const struct modern_shadow_vertex *v, int nv, const unsigned short *i, int ni, int model) { return 0; }
int modern_shadow_draw(int mesh, const float *m, int n, const void *a, const float uv[8], int views) { return 0; }
int modern_shadow_begin(int slot, const float p[3], float r, int soft, int ground_grenade) { return 0; }
int modern_shadow_end(void) { return 0; }
void modern_shadow_clear_draws(void) {}
void modern_shadow_change_bsp(void) {}
void modern_environment_invalidate_static(void) {}
void modern_shadow_dispose(void) {}
void modern_shadow_emit(struct xgpu_text *text) {}
void modern_shadow_uniforms_initialize(unsigned long p, struct modern_shadow_uniforms *u) { u->light = -1; }
void modern_shadow_bind(struct modern_shadow_uniforms *u) {}
#endif
