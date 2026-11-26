/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

#ifndef SHIELD_GLOW_H
#define SHIELD_GLOW_H
boolean shield_glow_enabled(void);
void shield_glow_reset(void);
void shield_glow_update(void);
void shield_glow_frame_end(void);
void shield_glow_window_begin(void);
void shield_glow_submitted(long light, long rasterizer);
boolean shield_glow_selected(long rasterizer);
long shield_glow_shadow_owner(long light);
float shield_glow_radius(long light, float radius);
#endif
