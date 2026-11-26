/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

#ifndef AMMO_COUNTER_LIGHTING_H
#define AMMO_COUNTER_LIGHTING_H

/* Pose result: 0 unavailable, 1 world model, 2 first-person model. */
int ammo_counter_lighting_position(long weapon, real_point3d *position, real_vector3d *direction);

struct shader;
void ammo_counter_lighting_screen(struct shader *shader, real_rgb_color *color);
/* Balanced around one model material draw, before any following fog pass. */
void ammo_counter_lighting_draw_begin(struct shader *shader, unsigned long output);
void ammo_counter_lighting_draw_end(void);
void ammo_counter_lighting_forget(void);

#endif
