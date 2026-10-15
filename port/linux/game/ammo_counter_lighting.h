#ifndef AMMO_COUNTER_LIGHTING_H
#define AMMO_COUNTER_LIGHTING_H

int ammo_counter_lighting_position(long weapon, real_point3d *position, real_vector3d *direction);

struct shader;
void ammo_counter_lighting_screen(struct shader *shader, real_rgb_color *color);
void ammo_counter_lighting_forget(void);

#endif
