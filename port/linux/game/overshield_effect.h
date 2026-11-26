#ifndef OVERSHIELD_EFFECT_H
#define OVERSHIELD_EFFECT_H
void overshield_effect_reset(void);
boolean overshield_effect_light_sample(long unit_index, real_rgb_color *color, real *coverage);
#endif

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
