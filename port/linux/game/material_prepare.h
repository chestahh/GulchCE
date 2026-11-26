/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

/* Loading-only material selection and offscreen desktop shader preparation. */
#ifndef MATERIAL_PREPARE_H
#define MATERIAL_PREPARE_H
enum { MATERIAL_PREPARE_STOCK = 1, MATERIAL_PREPARE_SHADOWS = 2 };
struct bitmap_data;
struct shader;
void material_prepare_map(void);
int material_prepare_draw(void);
int material_prepare_skip_shader(struct shader const *shader);
int material_prepare_bitmap(struct bitmap_data *bitmap, void **header);
int render_material_prepare_mode(int enabled);
void render_material_prepare_draw(void);
int texture_cache_prepare_descriptor(struct bitmap_data *bitmap, void *header);
#endif
