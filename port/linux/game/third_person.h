#ifndef THIRD_PERSON_H
#define THIRD_PERSON_H

struct render_camera;
union point2d;
union real_point3d;
union real_vector3d;
boolean third_person_muzzle_shot(long weapon_index, long unit_index, short trigger_index,
	union real_point3d *origin, union real_vector3d *forward);
void third_person_muzzle_target(short local_player_index, union real_point3d *target);
boolean third_person_free_look_input(short local_player_index, real yaw, real pitch);
void third_person_reset(void);
boolean third_person_active(short local_player_index);
void third_person_camera(short local_player_index, struct render_camera *camera);
boolean third_person_reticle(union point2d *point, real reticle_scale);

#endif
