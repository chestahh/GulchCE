/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

#ifndef COVENANT_EMITTER_LIGHTING_H
#define COVENANT_EMITTER_LIGHTING_H

int covenant_emitter_lighting_enabled(void);
void covenant_emitter_lighting_update(void);
void covenant_emitter_lighting_frame_end(void);
void covenant_emitter_lighting_forget(void);
void covenant_emitter_lighting_window_begin(void);
void covenant_emitter_lighting_submitted(long light, long rasterizer);
int covenant_emitter_lighting_selected(long rasterizer);
int covenant_emitter_lighting_vehicle_occlusion(long light);
long covenant_emitter_lighting_occlusion_ignore(long light);
long covenant_emitter_lighting_shadow_owner(long light, long *emitter_object);
float covenant_emitter_lighting_radius(long light, float radius);

/* Returns 0 for no mask, 1 for the AR screen, 2 for exposed needles. */
int covenant_emitter_lighting_cone(long rasterizer, float axis[3], float origin[3], float *radius);

#endif
