/* Optional single-light rendered shadow maps. Game and GL ownership stay
   separate; disabled/GLES builds never allocate shadow resources. */
#ifndef MODERN_SHADOWS_H
#define MODERN_SHADOWS_H

void modern_shadows_prepare(int primary);
void modern_shadows_reset(void);
void modern_shadows_window(void);
void modern_shadows_submitted(long identity, long rasterizer_index);
void modern_shadows_model_slot(long rasterizer_index, int slot);
void modern_shadows_world_begin(long rasterizer_index, int stage);
void modern_shadows_world_end(void);
unsigned int modern_shadows_model_mask(void);
int modern_shadows_selected(long rasterizer_index);
void modern_shadows_cancel(void);
int modern_shadows_world_stage(void);
float modern_shadows_world_extent(void);

struct modern_shadow_vertex
{
	float position[3];
	float nodes[2];
	float weight;
	float uv[2];
};

/* Meshes are immutable until reset. Matrices are column-major world poses;
   each draw copies them, so no borrowed object state survives the frame. */
int modern_shadow_mesh_create(const struct modern_shadow_vertex *vertices, int vertex_count,
	const unsigned short *indices, int index_count);
int modern_shadow_draw(int mesh, const float *matrices, int node_count,
	const void *alpha_texture, const float uv_scale[2]);
int modern_shadow_begin(const float position[3], float radius, int soft);
int modern_shadow_end(void);
void modern_shadow_dispose(void);
void modern_shadow_clear_draws(void);

struct xgpu_text;
struct modern_shadow_uniforms
{
	int light, slots, filter, extent;
};
void modern_shadow_emit(struct xgpu_text *text);
void modern_shadow_uniforms_initialize(unsigned long program, struct modern_shadow_uniforms *uniforms);
void modern_shadow_bind(const struct modern_shadow_uniforms *uniforms);

#endif
