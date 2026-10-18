#ifndef SHIELD_LOOK_H
#define SHIELD_LOOK_H

/* Local draw scope; no emulated render state or persistent unit state. */
int shield_look_enabled(void);
struct shader;
int shield_look_eligible(long object_index, struct shader const *shader,
	float const *values, int first_person);
int shield_look_visible(long object_index, struct shader const *shader,
	float const *values, int first_person);
void shield_look_begin(long object_index, struct shader const *shader,
	float const *values, int first_person, float time, float intensity,
	float const *color, float const *tint_color, float parallel_alpha, float perpendicular_alpha);
void shield_look_end(void);
void shield_look_set(float const *state, float const *color, float const *rim);
int shield_look_active(void);
float const *shield_look_uniforms(void);
struct xgpu_text;
void shield_look_emit(struct xgpu_text *text);

#endif
