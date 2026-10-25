/* Synchronous render-only ray batches; no state survives end(). */
#ifndef PLASMA_COLLISION_H
#define PLASMA_COLLISION_H
union real_point3d;
union real_vector3d;
void plasma_collision_begin(unsigned long flags, union real_point3d const *point, long ignore);
float plasma_collision_depth(unsigned long flags, union real_point3d const *point,
	union real_vector3d const *vector, long ignore);
void plasma_collision_end(void);
#endif
