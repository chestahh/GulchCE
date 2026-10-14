/* Bounded, render-only collision visibility for plasma lights. */
#ifndef PLASMA_OCCLUSION_H
#define PLASMA_OCCLUSION_H
void plasma_occlusion_reset(void);
void plasma_occlusion_window(void);
void plasma_occlusion_submit(long light, long rasterizer_index);
void plasma_occlusion_model_slot(long rasterizer_index, short slot);
void *plasma_occlusion_texture(long rasterizer_index, int specular);
void *plasma_occlusion_model_texture(int slot, float transform[4]);
void plasma_occlusion_bind_model(unsigned long program);
float plasma_occlusion_specular_scale(long rasterizer_index);
#endif
