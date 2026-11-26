/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

#ifndef AUTHORED_EMITTER_LIGHTING_H
#define AUTHORED_EMITTER_LIGHTING_H

enum { AUTHORED_EMITTER_PLASMA_GRENADE = 3, AUTHORED_EMITTER_BANSHEE_WING = 5 };

/* Exact object/light pairs; zero means the stock submission is untouched. */
int authored_emitter_lighting_enabled(void);
int authored_emitter_lighting_category_enabled(int category);
int authored_emitter_lighting_profile(long object, long definition, float *scale, float *forward_offset, int *owner_unshadowed);
void authored_emitter_wing_position(long light, long object, real_point3d *position);
void authored_emitter_lighting_forget(void);

#endif
