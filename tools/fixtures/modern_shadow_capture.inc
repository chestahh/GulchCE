/* Capture executable only. Fixed subjects and a moving real projectile light
   separate emitter placement from model casting and receiving. */
#include "render/render_cameras.h"
#include "game/game_globals.h"
static long shadow_subject = NONE, shadow_weapon = NONE;
static real_point3d shadow_anchor;
static real_vector3d shadow_forward, shadow_right;
static struct observer_result shadow_camera;
static int shadow_ready;
static long shadow_receiver = NONE;

static int shadow_floor(real_point3d *point)
{
	struct collision_result hit;
	real_point3d start = *point;
	real_vector3d down = {0, 0, -80};
	start.z += 1.0f;
	if (!collision_test_vector(FLAG(_collision_test_structure_bit) |
		FLAG(_collision_test_front_facing_surfaces_bit), &start, &down, NONE, &hit) || hit.plane.n.k < 0.7f)
		return FALSE;
	*point = hit.point;
	point->z += 0.03f;
	return TRUE;
}

static long shadow_place(long definition, real_point3d const *position,
	real_vector3d const *forward)
{
	struct object_placement_data placement;
	if (definition == NONE) return NONE;
	object_placement_data_new(&placement, definition, NONE);
	placement.position = *position;
	placement.forward = *forward;
	observer_up_from_forward(forward, &placement.up);
	return object_new(&placement);
}

static long shadow_create(long group, const char *name, real_point3d const *position,
	real_vector3d const *forward)
{
	long definition = tag_loaded(group, name);
	if (definition == NONE) platform_log("SHADOW SCENE missing %s", name);
	return shadow_place(definition, position, forward);
}

int modern_capture_surface_object(long object)
{
	return shadow_ready && object == shadow_subject && capture_ticks == 160;
}

static int shadow_wall(void)
{
	struct collision_result hit;
	real_vector3d ray;
	real best = 100.0f;
	int i, found = FALSE;
	for (i = 0; i < 16; i++)
	{
		real angle = i * 0.392699082f;
		ray.i = 64.0f * cos(angle);
		ray.j = 64.0f * sin(angle);
		ray.k = 0;
		if (collision_test_vector(FLAG(_collision_test_structure_bit) |
			FLAG(_collision_test_front_facing_surfaces_bit), &capture_camera.position,
			&ray, NONE, &hit) && fabs(hit.plane.n.k) < 0.2f && hit.t < best)
		{
			real_point3d floor = hit.point;
			real gap = getenv("HALO_SHADOW_CAPTURE_WALL_GAP") ? (real)atof(getenv("HALO_SHADOW_CAPTURE_WALL_GAP")) : 0.7f;
			struct collision_result wall;
			real_vector3d toward = {-hit.plane.n.i, -hit.plane.n.j, 0};
			floor.x += hit.plane.n.i * gap;
			floor.y += hit.plane.n.j * gap;
			if (!shadow_floor(&floor)) continue;
			floor.z += 0.4f;
			if (!collision_test_vector(FLAG(_collision_test_structure_bit) |
				FLAG(_collision_test_front_facing_surfaces_bit), &floor, &toward, NONE, &wall) || fabs(wall.plane.n.k) > 0.2f)
				continue;
			floor.z -= 0.4f;
			best = hit.t;
			shadow_anchor = floor;
			shadow_forward = hit.plane.n;
			found = TRUE;
		}
	}
	if (!found) return FALSE;
	platform_log("SHADOW SCENE wall %.5f %.5f %.5f normal %.5f %.5f %.5f",
		shadow_anchor.x, shadow_anchor.y, shadow_anchor.z,
		shadow_forward.i, shadow_forward.j, shadow_forward.k);
	return TRUE;
}

static void modern_capture_tick(const char *scene)
{
	struct object_datum *subject;
	real_point3d light, target;
	real angle, reach, height;
	long player = player_control_get_unit_index(0);
	int vehicle = !strcmp(scene, "warthog") || !strncmp(scene, "ghost", 5);
	int jackal = !strcmp(scene, "jackal-wall");
	int local = !strcmp(scene, "player") || !strcmp(scene, "first-person");
	int dropped = !strcmp(scene, "dropped-weapon");
	int grenade = !strcmp(scene, "frag-grenade") || !strcmp(scene, "plasma-grenade");
	int item = dropped || grenade;
	int external = strcmp(scene, "ghost-engine") && !jackal;
	int i;
	if (capture_ticks == 65)
	{
		shadow_ready = FALSE;
		shadow_subject = shadow_weapon = NONE;
		capture_shadow_light = NONE;
		shadow_forward = capture_camera.forward;
		shadow_forward.k = 0;
		normalize3d(&shadow_forward);
		shadow_anchor = capture_camera.position;
		reach = getenv("HALO_PLASMA_CAPTURE_DISTANCE") ? (real)atof(getenv("HALO_PLASMA_CAPTURE_DISTANCE")) : 7.0f;
		shadow_anchor.x += shadow_forward.i * reach;
		shadow_anchor.y += shadow_forward.j * reach;
		capture_ground(&shadow_anchor);
		if ((!strcmp(scene, "marine") || vehicle) && !shadow_floor(&shadow_anchor))
		{
			platform_log("SHADOW SCENE failed no floor");
			return;
		}
		if (jackal && !shadow_wall())
		{
			platform_log("SHADOW SCENE failed no wall");
			return;
		}
		cross_product3d(&shadow_forward, global_up3d, &shadow_right);
		if (local)
		{
			if (player == NONE) return;
			shadow_subject = player;
			object_set_position(player, &shadow_anchor, &shadow_forward, global_up3d);
			player_control_set_facing(0, &shadow_forward);
		}
		else if (grenade)
		{
			int type = !strcmp(scene, "plasma-grenade") ? 1 : 0;
			struct tag_block const *types = &scenario_get_game_globals()->grenades;
			if (type >= types->count) return;
			shadow_subject = shadow_place(TAG_BLOCK_GET_ELEMENT(types, type, struct game_globals_grenade)->item.index,
				&shadow_anchor, &shadow_forward);
		}
		else if (dropped)
			shadow_subject = shadow_create('weap', "weapons\\assault rifle\\assault rifle", &shadow_anchor, &shadow_forward);
		else
			shadow_subject = shadow_create(vehicle ? 'vehi' : 'bipd',
				vehicle && getenv("HALO_SHADOW_CAPTURE_VEHICLE") ? getenv("HALO_SHADOW_CAPTURE_VEHICLE") :
				!strcmp(scene, "warthog") ? "vehicles\\warthog\\warthog" :
				vehicle ? "vehicles\\ghost\\ghost_mp" : jackal ? "characters\\jackal\\jackal" :
				!strcmp(scene, "marine") ? (getenv("HALO_SHADOW_CAPTURE_HOLDER") ?
				getenv("HALO_SHADOW_CAPTURE_HOLDER") : "characters\\marine_armored\\marine_armored") :
				"characters\\cyborg\\cyborg", &shadow_anchor, &shadow_forward);
		if (shadow_subject == NONE) return;
		if (!strcmp(scene, "ghost-engine"))
		{
			long driver = shadow_create('bipd', "characters\\cyborg\\cyborg", &shadow_anchor, &shadow_forward);
			if (driver == NONE) return;
			unit_scripting_enter_vehicle(driver, shadow_subject, "G-driver");
			if (object_get(driver)->object.parent_object_index != shadow_subject)
			{
				platform_log("SHADOW SCENE failed driver seat");
				return;
			}
			platform_log("SHADOW SCENE driver %ld parent %ld", driver, shadow_subject);
		}
		if (!vehicle && !item && (!jackal || getenv("HALO_SHADOW_CAPTURE_WEAPON")))
		{
			unit_drop_current_weapon(shadow_subject, TRUE);
			unit_drop_current_weapon(shadow_subject, TRUE);
			shadow_weapon = shadow_create('weap', getenv("HALO_SHADOW_CAPTURE_WEAPON") ?
				getenv("HALO_SHADOW_CAPTURE_WEAPON") : "weapons\\assault rifle\\assault rifle", &shadow_anchor, &shadow_forward);
			if (shadow_weapon == NONE || !unit_add_weapon_to_inventory(shadow_subject, shadow_weapon, TRUE))
			{
				platform_log("SHADOW SCENE failed equip");
				return;
			}
			if (local)
				for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++)
					if (unit_inventory_get_weapon(player, (short)i) == shadow_weapon)
						player_control_set_desired_weapon(player, (short)i);
		}
		if (external)
		{
			light = shadow_anchor;
			light.z += 1.0f;
			capture_shadow_light = shadow_create('proj', "weapons\\plasma rifle\\charged bolt", &light, &shadow_forward);
			if (capture_shadow_light == NONE) return;
		}
		shadow_camera = capture_camera;
		target = shadow_anchor;
		target.z += vehicle ? 0.35f : item ? 0.03f : 0.45f;
		reach = vehicle ? 3.8f : jackal ? 2.3f : grenade ? 0.55f : dropped ? 0.85f : 1.7f;
		shadow_camera.position.x = target.x + reach * shadow_forward.i + (vehicle ? 2.5f : item ? 0.25f : 0.5f) * shadow_right.i;
		shadow_camera.position.y = target.y + reach * shadow_forward.j + (vehicle ? 2.5f : item ? 0.25f : 0.5f) * shadow_right.j;
		shadow_camera.position.z = target.z + (vehicle ? 1.0f : item ? 0.4f : 0.25f);
		if (getenv("HALO_SHADOW_CAPTURE_CAMERA_ANGLE"))
		{
			real x = shadow_camera.position.x - target.x;
			real y = shadow_camera.position.y - target.y;
			angle = (real)atof(getenv("HALO_SHADOW_CAPTURE_CAMERA_ANGLE")) * 0.0174532925f;
			shadow_camera.position.x = target.x + x * cos(angle) - y * sin(angle);
			shadow_camera.position.y = target.y + x * sin(angle) + y * cos(angle);
		}
		vector_from_points3d(&shadow_camera.position, &target, &shadow_camera.forward);
		normalize3d(&shadow_camera.forward);
		observer_up_from_forward(&shadow_camera.forward, &shadow_camera.up);
		scenario_location_from_point(&shadow_camera.location, &shadow_camera.position);
		shadow_ready = TRUE;
		if (jackal && player != NONE)
		{
			real_point3d floor = shadow_camera.position;
			if (shadow_floor(&floor)) object_set_position(player, &floor, &shadow_forward, global_up3d);
		}
		platform_log("SHADOW SCENE created %s subject %ld weapon %ld light %ld player %ld camera %.5f %.5f %.5f forward %.5f %.5f %.5f",
			scene, shadow_subject, shadow_weapon, capture_shadow_light, player,
			shadow_camera.position.x, shadow_camera.position.y, shadow_camera.position.z,
			shadow_camera.forward.i, shadow_camera.forward.j, shadow_camera.forward.k);
	}
	if (!shadow_ready || !(subject = object_try_and_get(shadow_subject))) return;
	if (capture_ticks == 80 && !strcmp(scene, "corpse")) unit_kill_silent(shadow_subject);
	if (external)
	{
		if (!object_try_and_get(capture_shadow_light))
		{
			platform_log("SHADOW SCENE failed light removed");
			shadow_ready = FALSE;
			return;
		}
		/* The same tick-indexed path in all modes; interpolation remains live. */
		angle = (PIN(capture_ticks, 100, 220) - 100) * (3.14159265f / 120.0f) - 1.57079633f;
		reach = vehicle ? 1.7f : item ? 0.45f : 0.8f;
		height = vehicle ? 0.8f : item ? 0.22f : !strcmp(scene, "corpse") ? 0.6f : 0.65f;
		if (getenv("HALO_PLASMA_CAPTURE_HEIGHT")) height = (real)atof(getenv("HALO_PLASMA_CAPTURE_HEIGHT"));
		light = shadow_anchor;
		light.x += reach * (cos(angle) * shadow_forward.i + sin(angle) * shadow_right.i);
		light.y += reach * (cos(angle) * shadow_forward.j + sin(angle) * shadow_right.j);
		light.z += height + 0.25f * cos(angle);
		if (getenv("HALO_SHADOW_CAPTURE_COMPOSITION"))
		{
			/* Move the real source through its ordinary falloff. No shadow or
			   light strength is overridden, and the subject stays in place. */
			real phase = PIN((capture_ticks - 100) / 120.f, 0.f, 1.f);
			real approach = sin(phase * 3.14159265f);
			real distance = 12.f - 10.5f * approach;
			light = shadow_anchor;
			light.x += distance * shadow_forward.i + (phase * 2.f - 1.f) * shadow_right.i;
			light.y += distance * shadow_forward.j + (phase * 2.f - 1.f) * shadow_right.j;
			light.z += 1.2f;
		}
		object_set_position(capture_shadow_light, &light, NULL, NULL);
	}
	if (capture_ticks >= 100 && capture_ticks <= 220 && capture_ticks % 30 == 10)
	{
		platform_log("SHADOW SCENE sample %ld subject %.5f %.5f %.5f body %.5f flags %lu weapon_parent %ld",
			capture_ticks, subject->object.position.x, subject->object.position.y, subject->object.position.z,
			subject->object.body_vitality, subject->object.damage_flags,
			shadow_weapon != NONE && object_try_and_get(shadow_weapon) ? object_get(shadow_weapon)->object.parent_object_index : NONE);
		if (external) platform_log("SHADOW SCENE light %ld %.5f %.5f %.5f", capture_ticks, light.x, light.y, light.z);
		if (!strcmp(scene, "ghost-engine"))
			platform_log("SHADOW SCENE engine %ld %.5f %.5f %.5f %.5f", capture_ticks,
				subject->object.outgoing_function_values[0], subject->object.outgoing_function_values[1],
				subject->object.outgoing_function_values[2], subject->object.outgoing_function_values[3]);
		if (vehicle && capture_ticks == 160)
		{
			struct object_definition *definition = object_definition_get(subject->definition_index);
			for (i = 0; i < definition->object.attachments.count; i++)
			{
				struct object_attachment_definition *attachment = TAG_BLOCK_GET_ELEMENT(&definition->object.attachments, i, struct object_attachment_definition);
				if (attachment->type.index != NONE)
					platform_log("SHADOW SCENE attachment %d marker '%s' tag %s functions %d %d", i,
						attachment->marker_name, tag_get_name(attachment->type.index),
						attachment->primary_scale_function_reference, attachment->secondary_scale_function_reference);
			}
		}
	}
}

static struct observer_result const *modern_capture_view(struct observer_result const *source)
{
	return shadow_ready && strcmp(getenv("HALO_SHADOW_CAPTURE_SCENE"), "first-person") ? &shadow_camera : source;
}

/* The ordinary shoulder camera runs after observer interpolation. Override
   only the final capture camera, leaving player visibility/perspective live. */
void modern_capture_camera(struct render_camera *camera)
{
	const char *scene = getenv("HALO_SHADOW_CAPTURE_SCENE");
	if (!scene || !shadow_ready || !strcmp(scene, "first-person")) return;
	camera->position = shadow_camera.position;
	camera->forward = shadow_camera.forward;
	camera->up = shadow_camera.up;
}

void modern_capture_receiver(long object)
{
	shadow_receiver = object;
}

/* Diagnostic counterfactual: keep visible geometry and lighting identical,
   removing only one caster to identify its shadow on another surface. */
int modern_capture_skip_caster(long object)
{
	const char *control = getenv("HALO_SHADOW_CAPTURE_OMIT");
	long vehicle = shadow_ready ? shadow_subject : capture_vehicle;
	struct object_datum *datum;
	int depth;
	if (!control || object == NONE) return FALSE;
	if (!strcmp(control, "vehicle")) return vehicle != NONE && object == vehicle;
	if (!strcmp(control, "occupants"))
	{
		if (vehicle == NONE || object == vehicle) return FALSE;
		for (depth = 0; depth < 16 && (datum = object_try_and_get(object)) != NULL; depth++)
		{
			object = datum->object.parent_object_index;
			if (object == vehicle) return TRUE;
		}
		return FALSE;
	}
	if (!shadow_ready) return FALSE;
	return (!strcmp(control, "weapon") && object == shadow_weapon) ||
		(!strcmp(control, "body") && object == shadow_subject);
}

void modern_capture_shader(unsigned long shader, int recognised, int registered, int pixel, int shadow, unsigned int mask)
{
	static long last_tick = -1;
	static unsigned long seen[64];
	static long objects[64];
	static int count;
	int i;
	if (!shadow_ready || shadow_receiver == NONE ||
		(shadow_receiver != shadow_subject && shadow_receiver != shadow_weapon) ||
		capture_ticks < 100 || capture_ticks > 220 || capture_ticks % 30 != 10) return;
	if (last_tick != capture_ticks) { last_tick = capture_ticks; count = 0; }
	for (i = 0; i < count; i++) if (seen[i] == shader && objects[i] == shadow_receiver) return;
	if (count == 64) return;
	objects[count] = shadow_receiver;
	seen[count++] = shader;
	platform_log("SHADOW SCENE receiver %ld object %ld shader %lu recognised %d registered %d pixel %d shadow %d lights %u",
		capture_ticks, shadow_receiver, shader, recognised, registered, pixel, shadow, mask);
}

void modern_capture_caster(long object, int views)
{
	static long last[2] = {-1, -1};
	int slot = object == shadow_subject ? 0 : object == shadow_weapon ? 1 : -1;
	if (!shadow_ready || slot < 0 || capture_ticks < 100 || capture_ticks > 220 ||
		capture_ticks % 30 != 10 || last[slot] == capture_ticks) return;
	last[slot] = capture_ticks;
	platform_log("SHADOW SCENE caster %ld object %ld views %d", capture_ticks, object, views);
	if (slot == 0 && capture_ticks == 160)
	{
		static const char *names[] = {"shield light", "running lights", "hover thrusters", "rodflare", "lens flare"};
		struct object_marker markers[8];
		int name, i, count;
		for (name = 0; name < NUMBEROF(names); name++)
		{
			count = object_get_marker_by_name(object, names[name], markers, NUMBEROF(markers));
			for (i = 0; i < count; i++)
			{
				real_matrix4x3 const *m = &markers[i].matrix;
				platform_log("SHADOW MARKER %s %d p %.6f %.6f %.6f f %.6f %.6f %.6f l %.6f %.6f %.6f u %.6f %.6f %.6f scale %.6f",
					names[name], i, m->position.x, m->position.y, m->position.z,
					m->forward.i, m->forward.j, m->forward.k, m->left.i, m->left.j, m->left.k,
					m->up.i, m->up.j, m->up.k, m->scale);
			}
		}
	}
}

/* Compare the actual visible model's relative pose with the pose submitted to
   the shadow GPU. Only capture builds call these; no production bookkeeping. */
static float shadow_pose[64 * 16];
static int shadow_pose_nodes;
static long shadow_pose_tick = -1;

void modern_capture_shadow_pose(long object, float const *matrices, int nodes)
{
	if (!shadow_ready || object != shadow_subject || nodes < 1 || nodes > 64) return;
	memcpy(shadow_pose, matrices, nodes * 16 * sizeof(float));
	shadow_pose_nodes = nodes;
	shadow_pose_tick = capture_ticks;
}

void modern_capture_visible_pose(long object, void const *pose, int nodes)
{
	real_matrix4x3 const *matrices = pose;
	static long last = -1;
	float maximum = 0;
	int node, row, column;
	if (!shadow_ready || object != shadow_subject || capture_ticks < 100 ||
		capture_ticks > 220 || capture_ticks % 30 != 10 || last == capture_ticks) return;
	last = capture_ticks;
	if (shadow_pose_tick != capture_ticks || nodes != shadow_pose_nodes)
	{
		platform_log("SHADOW POSE missing tick %ld nodes %d shadow_nodes %d shadow_tick %ld",
			capture_ticks, nodes, shadow_pose_nodes, shadow_pose_tick);
		return;
	}
	for (node = 0; node < nodes; node++)
	{
		real_matrix4x3 const *m = &matrices[node];
		float const *s = shadow_pose + node * 16;
		/* The visible shader consumes rows; the shadow shader consumes GL
		   columns. Compare their transformation coefficients explicitly. */
		for (row = 0; row < 3; row++)
		{
			for (column = 0; column < 3; column++)
			{
				float value = ((float const *)&m->forward)[column * 3 + row] * m->scale;
				maximum = MAX(maximum, (float)fabs(value - s[column * 4 + row]));
			}
			maximum = MAX(maximum, (float)fabs(((float const *)&m->position)[row] - s[12 + row]));
		}
	}
	platform_log("SHADOW POSE tick %ld nodes %d maximum_error %.9g", capture_ticks, nodes, maximum);
}
