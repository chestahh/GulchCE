#ifndef HALO_VULKAN_GAME_H
#define HALO_VULKAN_GAME_H
#include "platform.h"
extern int halo_vulkan_scene_active;
void halo_vk_game_viewport(const D3DVIEWPORT8 *viewport);
void halo_vk_game_target(D3DSurface *color, D3DSurface *depth);
void halo_vk_game_shutdown(void);
int halo_vk_game_create(unsigned width, unsigned height);
void halo_vk_game_transform(unsigned state, const D3DMATRIX *matrix, D3DMATRIX *result);
void halo_vk_game_constant_mode(unsigned mode);
void halo_vk_game_constants(int reg, const void *data, unsigned count);
void halo_vk_game_height(float a, float b, float c, float d);
void halo_vk_game_honeycomb(const float values[3][4]);
void halo_vk_game_texture(unsigned stage, D3DBaseTexture *texture);
void halo_vk_game_palette(unsigned stage, D3DPalette *palette);
void halo_vk_game_stream(unsigned stream, D3DVertexBuffer *buffer, unsigned stride);
void halo_vk_game_base(unsigned base);
HRESULT halo_vk_game_shader_create(const DWORD *decl, const DWORD *function, DWORD *handle);
void halo_vk_game_shader_set(DWORD handle);
void halo_vk_game_shader_load(DWORD handle, unsigned address);
void halo_vk_game_shader_select(DWORD handle, unsigned address);
unsigned halo_vk_game_shader_size(DWORD handle);
void halo_vk_game_draw(unsigned primitive, unsigned start, unsigned count, const WORD *indices);
void halo_vk_game_begin(unsigned primitive);
void halo_vk_game_end(void);
void halo_vk_game_attribute(int reg, float a, float b, float c, float d);
void halo_vk_game_clear(
	unsigned count, const D3DRECT *rects, unsigned flags, DWORD color, float z, unsigned stencil_value);
void halo_vk_game_present(void);
D3DSurface *halo_vk_game_surface(int depth);
#endif
