#ifndef AMMO_COUNTER_LIGHTING_H
#define AMMO_COUNTER_LIGHTING_H

/* Pose result: 0 unavailable, 1 world model, 2 first-person model. */
int ammo_counter_lighting_position(long weapon, real_point3d *position, real_vector3d *direction);

struct shader;
void ammo_counter_lighting_screen(struct shader *shader, real_rgb_color *color, int first_person);
void ammo_counter_lighting_forget(void);

#endif
