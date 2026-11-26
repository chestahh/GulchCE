/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

#ifndef NEEDLER_LIGHTING_H
#define NEEDLER_LIGHTING_H

int needler_lighting_position(long weapon, real_point3d *position, real_vector3d *direction);

struct shader;
void needler_lighting_material(struct shader *shader, real_rgb_color *color);
void needler_lighting_forget(void);
struct pixel_shader_definition;
void needler_lighting_transparent(struct shader *shader, struct pixel_shader_definition *pixel, int stages);

#endif
