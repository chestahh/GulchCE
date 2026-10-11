/* Optional shoulder view. Only the rendered camera changes: the director,
   network simulation stays stock. Offline shots use the world muzzle. */
#include "cseries.h"
#include "third_person.h"
#include "camera/director.h"
#include "cutscene/cinematics.h"
#include "game/players.h"
#include "units/units.h"
#include "physics/collisions.h"
#include "render/render.h"
#include "render/render_cameras_internal.h"
#include "main/main.h"
#include "camera/observer.h"
#include "halo_keyboard.h"

int config_boolean(char const *name);
unsigned long config_changes(void);
double config_real(char const *name);

struct shoulder_camera_state
{
	boolean initialized;
	long unit_index;
	real distance, height, side, boom;
	real_point3d eye;
	boolean looking;
	real look_yaw, look_pitch;
};
static struct shoulder_camera_state cameras[MAXIMUM_LOCAL_PLAYERS];
static boolean active[MAXIMUM_LOCAL_PLAYERS];
static boolean smooth_camera = TRUE;
static real requested_distance = 0.85f, requested_height = 0.12f, requested_side = 0.24f;

/* Reject non-finite values as well as out-of-range settings. */
static real camera_setting(char const *name, double low, double high, real fallback)
{
	double value = config_real(name);
	return value >= low && value <= high ? (real)value : fallback;
}

void third_person_reset(void)
{
	short index;
	for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
	{
		cameras[index].initialized = FALSE;
		cameras[index].looking = FALSE;
		cameras[index].look_yaw = cameras[index].look_pitch = 0.0f;
		active[index] = FALSE;
	}
}

static real_point3d targets[MAXIMUM_LOCAL_PLAYERS];

boolean third_person_active(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		active[local_player_index];
}

static boolean third_person_wanted(short local_player_index)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean enabled;
	long unit_index;
	struct unit_datum *unit;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		enabled = config_boolean("display.third_person") != 0;
		smooth_camera = config_boolean("display.third_person_smooth") != 0;
		requested_distance = camera_setting("display.third_person_distance", 0.3, 2.0, 0.85f);
		requested_height = camera_setting("display.third_person_height", -0.2, 0.6, 0.12f);
		requested_side = camera_setting("display.third_person_offset", 0.0, 0.6, 0.24f);
		if (config_boolean("display.third_person_left_shoulder"))
			requested_side = -requested_side;
	}
	if (!enabled || cinematic_in_progress() || *director_camera_scripted ||
		director_get_perspective(local_player_index) != _director_perspective_first_person ||
		director_inhibited_facing(local_player_index) ||
		player_control_get_zoom_level(local_player_index) != NONE)
		return FALSE;
	unit_index = player_control_get_unit_index(local_player_index);
	unit = unit_index != NONE ? unit_try_and_get(unit_index) : NULL;
	return unit && unit->object.parent_object_index == NONE &&
		!TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
		unit->unit.current_zoom_level == NONE && unit->unit.desired_zoom_level == NONE;
}

boolean third_person_free_look_input(short local_player_index, real yaw, real pitch)
{
	struct shoulder_camera_state *state;
	boolean held;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return FALSE;
	state = &cameras[local_player_index];
	held = (halo_keyboard_actions(local_player_index) & (1UL << HALO_KEYBOARD_FREE_LOOK)) != 0;
	if (!third_person_wanted(local_player_index) || !state->initialized ||
		state->unit_index != player_control_get_unit_index(local_player_index))
	{
		state->looking = FALSE;
		state->look_yaw = state->look_pitch = 0.0f;
		return FALSE;
	}
	state->looking = held && (state->looking || third_person_active(local_player_index));
	if (!state->looking)
		return FALSE;
	state->look_yaw += yaw;
	while (state->look_yaw > _pi) state->look_yaw -= 2.0f * _pi;
	while (state->look_yaw < -_pi) state->look_yaw += 2.0f * _pi;
	state->look_pitch = PIN(state->look_pitch + pitch, -_pi, _pi);
	return TRUE;
}

void third_person_camera(short local_player_index, struct render_camera *camera)
{
	long unit_index;
	struct unit_datum *unit;
	real_vector3d right, offset, ray, saved_forward, saved_up;
	real_euler_angles2d look_angles;
	real_point3d origin;
	struct collision_result collision;
	real fraction, length;
	real hard_fraction, blend, dt, wanted_boom;
	boolean was_active;
	struct shoulder_camera_state *state;
	unsigned long flags;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	was_active = active[local_player_index];
	active[local_player_index] = FALSE;
	state = &cameras[local_player_index];
	if (!camera || !third_person_wanted(local_player_index))
	{
		state->initialized = FALSE;
		state->looking = FALSE;
		state->look_yaw = state->look_pitch = 0.0f;
		return;
	}
	unit_index = player_control_get_unit_index(local_player_index);
	unit = unit_get(unit_index);
	dt = PIN(main_get_seconds_elapsed(), 0.0f, 0.1f);
	vector_from_points3d(&state->eye, &camera->position, &offset);
	if (!state->initialized || state->unit_index != unit_index || magnitude3d(&offset) > 2.0f)
	{
		state->looking = FALSE;
		state->look_yaw = state->look_pitch = 0.0f;
		state->distance = requested_distance;
		state->height = requested_height;
		state->side = requested_side;
		state->boom = 0.0f;
		state->unit_index = unit_index;
		state->initialized = TRUE;
		was_active = FALSE;
	}
	state->eye = camera->position;
	/* Input releases immediately even if gameplay is paused by a menu. */
	if (!(halo_keyboard_actions(local_player_index) & (1UL << HALO_KEYBOARD_FREE_LOOK)))
		state->looking = FALSE;
	if (!state->looking)
	{
		blend = smooth_camera ? dt / (0.12f + dt) : 1.0f;
		state->look_yaw *= 1.0f - blend;
		state->look_pitch *= 1.0f - blend;
		if (state->look_yaw * state->look_yaw + state->look_pitch * state->look_pitch < 0.000001f)
			state->look_yaw = state->look_pitch = 0.0f;
	}
	saved_forward = camera->forward;
	saved_up = camera->up;
	if (state->looking || state->look_yaw != 0.0f || state->look_pitch != 0.0f)
	{
		euler_angles2d_from_vector3d(&look_angles, &camera->forward);
		state->look_pitch = PIN(state->look_pitch,
			-DEGREES_TO_RADIANS(80.0f) - look_angles.pitch,
			DEGREES_TO_RADIANS(80.0f) - look_angles.pitch);
		look_angles.yaw += state->look_yaw;
		look_angles.pitch += state->look_pitch;
		vector3d_from_euler_angles2d(&camera->forward, &look_angles);
		observer_up_from_forward(&camera->forward, &camera->up);
	}
	blend = smooth_camera ? dt / (0.12f + dt) : 1.0f;
	state->distance += (requested_distance - state->distance) * blend;
	state->height += (requested_height - state->height) * blend;
	state->side += (requested_side - state->side) * blend;
	/* Halo world units are about three metres. Sweep the whole diagonal
	   from the eye so the shoulder cannot pass through a corner. */
	cross_product3d(&camera->forward, &camera->up, &right);
	normalize3d(&right);
	offset.i = -state->distance * camera->forward.i + state->side * right.i + state->height * camera->up.i;
	offset.j = -state->distance * camera->forward.j + state->side * right.j + state->height * camera->up.j;
	offset.k = -state->distance * camera->forward.k + state->side * right.k + state->height * camera->up.k;
	length = magnitude3d(&offset);
	fraction = 1.0f;
	flags = FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit) |
		FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) |
		_collision_test_objects_sight_blocking_flags;
	/* The sphere sweep covers the BSP; the ray also tests scenery,
	   doors and vehicles, which the engine's pill test does not visit. */
	if (collision_test_pill_new(flags, &camera->position, &offset, 0.06f, unit_index, &collision))
		fraction = PIN(collision.t - 0.02f / length, 0.0f, 1.0f);
	if (collision_test_vector(flags, &camera->position, &offset, unit_index, &collision))
		fraction = MIN(fraction, MAX(0.0f, collision.t - 0.06f / length));
	hard_fraction = fraction;
	/* Start drawing inward before the hard collision limit. Safety always
	   wins over smoothing; recovery is slower than pull-in. */
	if (collision_test_pill_new(flags, &camera->position, &offset, 0.10f, unit_index, &collision))
		fraction = MIN(fraction, MAX(0.0f, collision.t - 0.02f / length));
	/* A soft sweep can start inside a narrow doorway even when the hard
	   sweep fits. It must not itself force a change to first person. */
	if (hard_fraction * length >= 0.16f)
		fraction = MAX(fraction, 0.16f / length);
	wanted_boom = fraction * length;
	blend = smooth_camera ? dt / ((wanted_boom < state->boom ? 0.06f : 0.25f) + dt) : 1.0f;
	state->boom += (wanted_boom - state->boom) * blend;
	state->boom = MIN(state->boom, hard_fraction * length);
	/* Separate entry and exit thresholds prevent rapid perspective changes
	   when a wall leaves just enough room for the player's body. */
	if (hard_fraction * length < (was_active ? 0.10f : 0.14f))
	{
		/* Collision fallback must retain the player's first-person aim. */
		camera->forward = saved_forward;
		camera->up = saved_up;
		return;
	}
	state->boom = MAX(state->boom, MIN(0.14f, hard_fraction * length));
	point_from_line3d(&camera->position, &offset, state->boom / length, &camera->position);
	active[local_player_index] = TRUE;

	/* Project the stock firing line, not a ray from the displaced camera.
	   This indicates the nominal aim point; spread and projectile gravity
	   remain the weapon's own, just as in first person. */
	unit_get_camera_position(unit_index, &origin);
	scale_vector3d(&unit->unit.aiming_vector, 1000.0f, &ray);
	point_from_line3d(&origin, &ray, 1.0f, &targets[local_player_index]);
	if (collision_test_vector(_collision_test_for_projectiles_flags, &origin, &ray,
		unit_index, &collision))
		targets[local_player_index] = collision.point;
	third_person_muzzle_target(local_player_index, &targets[local_player_index]);
}

boolean third_person_reticle(point2d *point, real reticle_scale)
{
	real_point2d screen;
	real center_x, center_y;

	if (!third_person_active(render.local_player_index))
		return TRUE;
	if (cameras[render.local_player_index].looking ||
		cameras[render.local_player_index].look_yaw != 0.0f ||
		cameras[render.local_player_index].look_pitch != 0.0f)
		return FALSE;
	if (!render_camera_world_to_screen(&render.camera, &render.frustum,
		&targets[render.local_player_index], &screen))
		return FALSE;
	center_x = (render.camera.window_bounds.x0 + render.camera.window_bounds.x1) * 0.5f;
	center_y = (render.camera.window_bounds.y0 + render.camera.window_bounds.y1) * 0.5f;
	/* hud_draw_bitmap_internal scales about the original screen centre. */
	point->x += (short)((screen.x - center_x) / reticle_scale);
	point->y += (short)((screen.y - center_y) / reticle_scale);
	return TRUE;
}
