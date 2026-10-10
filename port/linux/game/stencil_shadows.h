#ifndef STENCIL_SHADOWS_H
#define STENCIL_SHADOWS_H

/* Optional render-only volumes; reset releases all feature-owned storage. */
void stencil_shadows_render(void);
void stencil_shadows_reset(void);

#endif
