/* Optional shoulder view. Only the rendered camera changes: the director,
   player controls, projectile origins and network simulation stay stock. */
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

int config_boolean(char const *name);
unsigned long config_changes(void);
double config_real(char const *name);

struct shoulder_camera_state
{
	boolean initialized;
	long unit_index;
	real distance, height, side, boom;
	real_point3d eye;
};
static struct shoulder_camera_state cameras[MAXIMUM_LOCAL_PLAYERS];
static boolean active[MAXIMUM_LOCAL_PLAYERS];
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

void third_person_camera(short local_player_index, struct render_camera *camera)
{
	long unit_index;
	struct unit_datum *unit;
	real_vector3d right, offset, ray;
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
		return;
	}
	unit_index = player_control_get_unit_index(local_player_index);
	unit = unit_get(unit_index);
	dt = PIN(main_get_seconds_elapsed(), 0.0f, 0.1f);
	vector_from_points3d(&state->eye, &camera->position, &offset);
	if (!state->initialized || state->unit_index != unit_index || magnitude3d(&offset) > 2.0f)
	{
		state->distance = requested_distance;
		state->height = requested_height;
		state->side = requested_side;
		state->boom = 0.0f;
		state->unit_index = unit_index;
		state->initialized = TRUE;
		was_active = FALSE;
	}
	state->eye = camera->position;
	blend = dt / (0.12f + dt);
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
	if (collision_test_pill_new(flags, &camera->position, &offset, 0.10f, unit_index, &collision))
		fraction = PIN(collision.t - 0.02f / length, 0.0f, 1.0f);
	if (collision_test_vector(flags, &camera->position, &offset, unit_index, &collision))
		fraction = MIN(fraction, MAX(0.0f, collision.t - 0.10f / length));
	hard_fraction = fraction;
	/* Start drawing inward before the hard collision limit. Safety always
	   wins over smoothing; recovery is slower than pull-in. */
	if (collision_test_pill_new(flags, &camera->position, &offset, 0.24f, unit_index, &collision))
		fraction = MIN(fraction, MAX(0.0f, collision.t - 0.02f / length));
	wanted_boom = fraction * length;
	blend = dt / ((wanted_boom < state->boom ? 0.06f : 0.25f) + dt);
	state->boom += (wanted_boom - state->boom) * blend;
	state->boom = MIN(state->boom, hard_fraction * length);
	/* Separate entry and exit thresholds prevent rapid perspective changes
	   when a wall leaves just enough room for the player's body. */
	if (state->boom < (was_active ? 0.20f : 0.28f))
		return;
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
}

boolean third_person_reticle(point2d *point, real reticle_scale)
{
	real_point2d screen;
	real center_x, center_y;

	if (!third_person_active(render.local_player_index))
		return TRUE;
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
