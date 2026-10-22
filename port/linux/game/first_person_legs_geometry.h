/*
FIRST_PERSON_LEGS_GEOMETRY.H

Private lower-body index buffers; original vertices and palettes stay owned
by their map.
*/
#ifndef FIRST_PERSON_LEGS_GEOMETRY_H
#define FIRST_PERSON_LEGS_GEOMETRY_H

struct model;
struct triangle_buffer;
struct vertex_buffer;

struct triangle_buffer const *first_person_legs_geometry_get(
	long definition_index, struct model const *model, boolean const *upper, boolean const *lower,
	struct vertex_buffer const *vertices, struct triangle_buffer const *triangles);
void first_person_legs_geometry_reset(void);

boolean first_person_legs_geometry_prepare(long definition_index, struct model const *model,
	boolean const *upper, boolean const *lower);
real first_person_legs_geometry_setback(long definition_index, real_matrix4x3 const *nodes,
	real_point3d const *pivot, real cosine, real sine, real_vector3d const *view,
	real_point3d const *eye, real vertical_fov, real minimum);
#endif
