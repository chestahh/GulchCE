/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

#ifndef AMBIENT_EMITTER_LIGHTING_H
#define AMBIENT_EMITTER_LIGHTING_H

/* Render samples share the existing bounded emitter pool. */
struct emitter_sample
{
	int kind, owner_unshadowed;
	real strength, radius, view_gain;
	real_rgb_color color;
	real_point3d position;
	real_vector3d direction;
};
enum { EMITTER_VEHICLE = 5, EMITTER_WORLD, EMITTER_FLAME };
int ambient_emitter_lighting_enabled(void);
/* Build bounded stream samples and return only the object types worth scanning. */
unsigned long ambient_emitter_lighting_begin(void);
void ambient_emitter_lighting_forget(void);
int ambient_emitter_lighting_sample(long object, int component, struct emitter_sample *sample);

#endif
