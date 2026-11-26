/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

#ifndef AMMO_COUNTER_FALLOFF_H
#define AMMO_COUNTER_FALLOFF_H

float ammo_counter_falloff_factor(const float axis[3], float x, float y, float z);
float needler_falloff_factor(float height);
void *ammo_counter_falloff_texture(long rasterizer, int radial, float transform[4]);
void ammo_counter_falloff_window(void);
void ammo_counter_falloff_dispose(void);

#endif
