/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

#ifndef PLASMA_LIGHTING_H
#define PLASMA_LIGHTING_H

union real_point3d;
union real_vector3d;

void plasma_lighting_prepare(long light_index, long object_index, long definition_index);
void plasma_lighting_attachment_position(long light_index, long object_index,
	union real_point3d *position, union real_vector3d const *forward, float scale);
float plasma_lighting_radius(long light_index, long object_index, float radius);
int light_port_attachment_radius(long light_index, long object_index, float *radius, int restore);
int plasma_lighting_vehicle_occlusion(long light_index, long *ignore);
struct plasma_shadow_source
{
	long casing;
	long stuck_host;
	long emitter_object;
	int grounded;
};

long plasma_lighting_shadow_owner(long light, struct plasma_shadow_source *grenade);
void plasma_lighting_frame_end(void);
void plasma_lighting_reset(void);
void plasma_lighting_window_begin(void);
void plasma_lighting_submitted(long light_index, long rasterizer_index);
int plasma_lighting_selected(long rasterizer_index);
void plasma_falloff_begin(long rasterizer_index);
void plasma_falloff_specular_begin(long rasterizer_index);
void plasma_specular_begin(long rasterizer_index);
unsigned long plasma_specular_color(unsigned long material);
void plasma_falloff_end(void);
void plasma_falloff_dispose(void);

/* object_lights.c: validate the owner before refreshing an attachment. */
int light_port_refresh_attachment(long light_index, long object_index);
int light_port_dynamic_set(long light_index, long object_index, int enabled);

#endif
