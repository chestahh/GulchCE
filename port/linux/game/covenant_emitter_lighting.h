#ifndef COVENANT_EMITTER_LIGHTING_H
#define COVENANT_EMITTER_LIGHTING_H

int covenant_emitter_lighting_enabled(void);
void covenant_emitter_lighting_update(void);
void covenant_emitter_lighting_frame_end(void);
void covenant_emitter_lighting_forget(void);
void covenant_emitter_lighting_window_begin(void);
void covenant_emitter_lighting_submitted(long light, long rasterizer);
int covenant_emitter_lighting_selected(long rasterizer);
long covenant_emitter_lighting_occlusion_ignore(long light);
float covenant_emitter_lighting_radius(long light, float radius);

#endif
