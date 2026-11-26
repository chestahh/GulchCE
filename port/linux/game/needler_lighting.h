/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

#ifndef NEEDLER_LIGHTING_H
#define NEEDLER_LIGHTING_H

int needler_lighting_position(long weapon, real_point3d *position, real_vector3d *direction);

struct shader;
void needler_lighting_material(struct shader *shader, real_rgb_color *color);
void needler_lighting_forget(void);
struct pixel_shader_definition;
void needler_lighting_transparent(struct shader *shader, struct pixel_shader_definition *pixel, int stages);

#endif
