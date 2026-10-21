/* Plain host/game ABI boundary. No platform, XDK or GL types cross it. */
#ifndef HALO_VULKAN_SCENE_H
#define HALO_VULKAN_SCENE_H
#include <stdint.h>
struct halo_vk_device;
struct halo_vk_present;
struct halo_vk_uniforms
{
	float c[192][4], viewport_scale[4], viewport_offset[4];
	float point_size, screen_offset, alpha_reference, reserved;
	float height_fade[4], ps_c0[8][4], ps_c1[8][4];
	float ps_final_c0[4], ps_final_c1[4], fog_color[4], fog_parameters[4];
	float bump_matrix[4][4], bump_luminance[4][4], texture_scale[4][4];
};
struct halo_vk_raster
{
	uint32_t topology, depth_test, depth_write, depth_function;
	uint32_t blend, source_blend, dest_blend, blend_op, color_mask, cull;
	uint32_t stencil, stencil_function, stencil_fail, stencil_zfail, stencil_pass;
	uint32_t stencil_ref, stencil_read, stencil_write, bias;
	float bias_slope, bias_units;
};
struct halo_vk_draw
{
	uint32_t color, depth, vertex_shader, pixel_shader;
	uint32_t textures[4], address[4][3], filter[4][3];
	float lod_bias[4], blend_color[4];
	float viewport[6];
	struct halo_vk_raster raster;
	struct halo_vk_uniforms uniforms;
};
struct halo_vk_texture_upload
{
	uint32_t width, height, depth, levels, faces, format;
	const void *pixels;
	uint32_t bytes;
	/* Tight image levels in Vulkan order: face, then mip. */
	uint32_t offsets[6][16];
};
int halo_vk_scene_open(struct halo_vk_device *device);
int halo_vk_scene_begin(void);
uint32_t halo_vk_scene_texture_update(const struct halo_vk_texture_upload *upload, uint32_t existing);
int halo_vk_scene_close(void);
int halo_vk_scene_owned(void);
uint32_t halo_vk_scene_shader(const char *glsl, int fragment);
uint32_t halo_vk_scene_texture(const struct halo_vk_texture_upload *upload);
uint32_t halo_vk_scene_target(uint32_t identity, unsigned width, unsigned height, int depth);
int halo_vk_scene_clear(uint32_t color, uint32_t depth, unsigned flags, const float rgba[4], float z,
	unsigned stencil, const int *rectangles, unsigned count);
int halo_vk_scene_draw_indexed(const struct halo_vk_draw *draw, const float *vertices, unsigned count,
	const unsigned *indices, unsigned index_count);
int halo_vk_scene_draw(const struct halo_vk_draw *draw, const float *vertices, unsigned count);
int halo_vk_scene_finish(uint32_t back, struct halo_vk_present *presentation, const char *capture);
int halo_vk_scene_failed(void);
void halo_vk_scene_error(const char *reason);
char *halo_vk_shader_interface(const char *source, int fragment);
void halo_vk_scene_query_begin(void);
int halo_vk_scene_query_end(unsigned index);
int halo_vk_scene_query_result(unsigned index, unsigned *pixels);
#endif
