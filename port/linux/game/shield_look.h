/* SHIELD LOOK: optional render-only profiles; CLASSIC preserves map-authored shields. */
#ifndef SHIELD_LOOK_H
#define SHIELD_LOOK_H

struct shader;
struct model;
struct render_model_effect;
union real_rgb_color;
struct real_matrix4x3;

boolean shield_look_colors(struct shader const *shader, union real_rgb_color const *colors,
	union real_rgb_color *perpendicular, union real_rgb_color *parallel);
void shield_arcs_unload(void);
void shield_arcs_draw(long object_index, long model_index, struct real_matrix4x3 const *nodes,
	struct render_model_effect const *effect, boolean arms);
void shield_recharge_reset(void);
void shield_recharge_prepare(long object_index, struct render_model_effect const *effect, boolean arms);
boolean shield_recharge_visible(struct shader const *shader, real const *values);
void shield_recharge_draw(struct shader const *shader, real const *values, real *intensity, real *offset);
real shield_look_honeycomb_scale(struct shader const *shader);
short shield_look_honeycomb_classic_mode(struct shader const *shader);
void shield_look_reset(void);
boolean shield_look_skip_shell(struct model const *model, struct shader const *surface);
struct shader *shield_look_body(long object_index, struct shader *original);
boolean shield_look_arms(short local_player_index, long unit_index, long hands_model_index,
	struct render_model_effect *effect);

#endif
