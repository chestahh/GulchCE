/* Test-build-only fixture. The visual runner installs and removes this file.
			Creates real plasma projectiles; never linked into the normal game. */
#include "cseries.h"
#include <xtl.h>
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "items/projectiles.h"
#include "items/projectile_definitions.h"
#include "camera/observer.h"
#include "tag_files/tag_groups.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_lights.h"
#include "plasma_lighting.h"
#include "game/players.h"
#include "game/game.h"
#include "units/units.h"
#include "devices/devices.h"
#include "cache/cache_files.h"
#include "cutscene/cinematics.h"
#include <stdlib.h>
#include <string.h>

void platform_log(const char *format, ...);
int config_boolean(const char *name);
real render_interpolation_fraction(void);
static long capture_ticks, capture_projectile = NONE, capture_last_time;
static struct observer_result capture_camera;
static int capture_model, capture_probe, capture_burst = 1;
static long capture_bolts[32], capture_door = NONE;
static int capture_combat, capture_toggle;
static long capture_target = NONE;
static const char *capture_projectile_tags[] = {
	"weapons\\plasma rifle\\bolt", "weapons\\plasma pistol\\bolt",
	"weapons\\plasma rifle\\charged bolt"
};
static int capture_kind(void)
{
	const char *kind = getenv("HALO_PLASMA_CAPTURE_KIND");
	if (kind && !strcmp(kind, "pistol")) return 1;
	if (kind && !strcmp(kind, "charged")) return 2;
	return 0;
}
int config_write_boolean(const char *name, int value);
static real capture_distance, capture_height, capture_receiver_distance, capture_span;
static real capture_option(const char *name, real fallback)
{
	const char *value = getenv(name);
	return value ? (real)atof(value) : fallback;
}
static unsigned long capture_model_draws, capture_specular_draws;
static int capture_specular_selected;
void plasma_capture_model_draw(unsigned int mask)
{
	if (mask)
		capture_model_draws++;
}
void plasma_capture_specular_begin(long index)
{
	capture_specular_selected = plasma_lighting_selected(index);
}
void plasma_capture_specular_draw(void)
{
	if (capture_specular_selected)
		capture_specular_draws++;
}

int plasma_capture_hold_projectile(long index)
{
	int n;
	for (n = 0; n < capture_burst; n++)
		if (capture_probe && index == capture_bolts[n])
			return 1;
	return 0;
}
void plasma_capture_tick(void)
{
	struct object_placement_data placement;
	real_vector3d right;
	long tag, i, bolt;
	long now = game_time_get();
	if (now < capture_last_time)
	{
		capture_ticks = 0;
		capture_projectile = capture_door = capture_target = NONE;
		for (i = 0; i < 32; i++)
			capture_bolts[i] = NONE;
	}
	capture_last_time = now;
	if (getenv("HALO_PLASMA_CAPTURE_DOOR") && cinematic_in_progress())
		return;
	capture_ticks++;
	if (capture_ticks == 60)
	{
		capture_camera = *observer_get_camera(0);
		capture_combat = getenv("HALO_PLASMA_CAPTURE_COMBAT") != NULL;
		capture_toggle = getenv("HALO_PLASMA_CAPTURE_TOGGLE") != NULL;
		capture_model = getenv("HALO_PLASMA_CAPTURE_MODEL") != NULL;
		capture_burst = (int)capture_option("HALO_PLASMA_CAPTURE_BURST", 1);
		if (capture_burst < 1 || capture_burst > 32)
			capture_burst = 1;
		for (i = 0; i < 32; i++)
			capture_bolts[i] = NONE;
		capture_probe = getenv("HALO_PLASMA_CAPTURE_PROBE") != NULL;
		capture_span = capture_option("HALO_PLASMA_CAPTURE_SPAN", 2.0f);
		capture_distance = capture_option("HALO_PLASMA_CAPTURE_DISTANCE", capture_model ? 2.2f : 3.0f);
		capture_height = capture_option("HALO_PLASMA_CAPTURE_HEIGHT", capture_model ? -0.2f : -0.6f);
		capture_receiver_distance = capture_option("HALO_PLASMA_CAPTURE_RECEIVER_DISTANCE", 2.8f);
		platform_log("PLASMA camera %.4f %.4f %.4f forward %.4f %.4f %.4f", capture_camera.position.x,
					 capture_camera.position.y, capture_camera.position.z, capture_camera.forward.i,
					 capture_camera.forward.j, capture_camera.forward.k);
	}
	if (capture_ticks == 65 && capture_combat)
	{
		long unit = player_control_get_unit_index(0);
		const char *role = getenv("HALO_NETWORK_TEST");
		tag = tag_loaded('weap', capture_kind() ? "weapons\\plasma pistol\\plasma pistol" : "weapons\\plasma rifle\\plasma rifle");
		if (unit != NONE && tag != NONE && (!role || strncmp(role, "join", 4)))
		{
			long weapon;
			if (role && !strncmp(role, "host", 4))
			{
				struct data_iterator players;
				struct player_datum *player;
				data_iterator_new(&players, player_data);
				while ((player = (struct player_datum *)data_iterator_next(&players)) != NULL)
					if (player->unit_index != NONE && player->unit_index != unit)
					{
						real_point3d position = object_get(unit)->object.position;
						position.x += capture_camera.forward.i * 4;
						position.y += capture_camera.forward.j * 4;
						object_set_position(player->unit_index, &position, NULL, NULL);
						platform_log("PLASMA combat target %ld", player->unit_index);
					}
			}
			unit_drop_current_weapon(unit, TRUE);
			unit_drop_current_weapon(unit, TRUE);
			object_placement_data_new(&placement, tag, NONE);
			placement.position = capture_camera.position;
			weapon = object_new(&placement);
			if (weapon != NONE && unit_add_weapon_to_inventory(unit, weapon, TRUE))
			{
				for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++)
					if (unit_inventory_get_weapon(unit, (short)i) == weapon)
						player_control_set_desired_weapon(unit, (short)i);
				platform_log("PLASMA combat equipped %ld tag %s", weapon, tag_get_name(tag));
			}
		}
	}
	if (capture_ticks == 70 && getenv("HALO_PLASMA_CAPTURE_DOOR"))
	{
		struct tag_iterator iterator;
		tag_iterator_new(&iterator, 'mach');
		while ((tag = tag_iterator_next(&iterator)) != NONE)
			if (strstr(tag_get_name(tag), "door"))
			{
				object_placement_data_new(&placement, tag, NONE);
				placement.position = capture_camera.position;
				placement.position.x += capture_camera.forward.i * 2.5f;
				placement.position.y += capture_camera.forward.j * 2.5f;
				placement.position.z -= 0.6f;
				placement.forward = capture_camera.forward;
				observer_up_from_forward(&placement.forward, &placement.up);
				capture_door = object_new(&placement);
				if (capture_door != NONE)
				{
					struct scenario_device_datum info;
					info.power_group_index = info.position_group_index = NONE;
					info.flags = 0;
					device_add_scenario_information(capture_door, &info);
					device_operates_automatically_set(capture_door, FALSE);
					device_set_power(capture_door, 1);
					device_set_actual_position(capture_door, 0);
				}
				platform_log("PLASMA door %ld tag %s", capture_door, tag_get_name(tag));
				break;
			}
	}
	if (capture_door != NONE && object_try_and_get(capture_door) && capture_ticks % 30 == 0)
	{
		device_set_desired_position(capture_door, (capture_ticks / 30) % 2 ? 1.0f : 0.0f);
		platform_log("PLASMA door position %.3f", device_get_position(capture_door));
	}
	if (capture_toggle && capture_ticks >= 100 && capture_ticks <= 220 && capture_ticks % 10 == 0)
	{
		config_write_boolean("display.plasma_occlusion", (capture_ticks / 10) % 2);
		platform_log("PLASMA toggle %d", config_boolean("display.plasma_occlusion"));
	}
	if (capture_combat)
		return;
	if (capture_ticks == 65 && capture_model)
	{
		tag = tag_loaded('bipd', "characters\\cyborg_mp\\cyborg_mp");
		if (tag == NONE)
			tag = tag_loaded('bipd', "characters\\cyborg\\cyborg");
		if (tag != NONE)
		{
			object_placement_data_new(&placement, tag, NONE);
			placement.position.x =
				capture_camera.position.x + capture_receiver_distance * capture_camera.forward.i;
			placement.position.y =
				capture_camera.position.y + capture_receiver_distance * capture_camera.forward.j;
			placement.position.z = capture_camera.position.z - 0.6f;
			placement.forward = capture_camera.forward;
			placement.forward.i = -placement.forward.i;
			placement.forward.j = -placement.forward.j;
			observer_up_from_forward(&placement.forward, &placement.up);
			capture_target = object_new(&placement);
			platform_log("PLASMA receiver model %ld tag %s", capture_target, tag_get_name(tag));
		}
		else
			platform_log("PLASMA receiver missing");
	}
	for (bolt = 0; bolt < capture_burst; bolt++)
		if (capture_probe && capture_bolts[bolt] != NONE && object_try_and_get(capture_bolts[bolt]))
		{
			real_point3d position;
			real lateral = -capture_span + 2 * capture_span * (capture_ticks % 60 - 30) / 28.0f;
			cross_product3d(&capture_camera.forward, &capture_camera.up, &right);
			position.x = capture_camera.position.x + capture_distance * capture_camera.forward.i +
						 lateral * right.i + capture_height * capture_camera.up.i;
			position.y = capture_camera.position.y + capture_distance * capture_camera.forward.j +
						 lateral * right.j + capture_height * capture_camera.up.j;
			position.z = capture_camera.position.z + capture_distance * capture_camera.forward.k +
						 lateral * right.k + capture_height * capture_camera.up.k;
			if (capture_ticks % 60 >= 59 || capture_ticks % 60 < 30)
			{
				object_delete(capture_bolts[bolt]);
				capture_bolts[bolt] = NONE;
			}
			else
			{
				position.z += bolt * 0.025f;
				object_set_position(capture_bolts[bolt], &position, NULL, NULL);
			}
		}
	if (capture_ticks < 90 || capture_ticks > 210 || capture_ticks % 60 != 30)
		return;
	tag = tag_loaded(PROJECTILE_DEFINITION_TAG, capture_projectile_tags[capture_kind()]);
	if (tag == NONE)
	{
		platform_log("PLASMA missing projectile tag");
		return;
	}
	cross_product3d(&capture_camera.forward, &capture_camera.up, &right);
	object_placement_data_new(&placement, tag, NONE);
	placement.position.x = capture_camera.position.x + capture_distance * capture_camera.forward.i -
						   (capture_probe ? capture_span : 1.2f) * right.i +
						   capture_height * capture_camera.up.i;
	placement.position.y = capture_camera.position.y + capture_distance * capture_camera.forward.j -
						   (capture_probe ? capture_span : 1.2f) * right.j +
						   capture_height * capture_camera.up.j;
	placement.position.z = capture_camera.position.z + capture_distance * capture_camera.forward.k -
						   (capture_probe ? capture_span : 1.2f) * right.k +
						   capture_height * capture_camera.up.k;
	placement.forward = right;
	placement.forward.k -= 0.12f;
	normalize3d(&placement.forward);
	observer_up_from_forward(&placement.forward, &placement.up);
	for (bolt = 0; bolt < capture_burst; bolt++)
	{
		placement.position.z += 0.025f;
		if (getenv("HALO_PLASMA_CAPTURE_MIXED"))
			placement.definition_index = tag_loaded(PROJECTILE_DEFINITION_TAG, capture_projectile_tags[bolt % 3]);
		capture_bolts[bolt] = object_new(&placement);
		if (capture_bolts[bolt] != NONE && capture_target != NONE && capture_kind() == 2 && !capture_probe)
		{
			projectile_set_target_object_index(capture_bolts[bolt], capture_target);
			platform_log("PLASMA homing %ld target %ld", capture_bolts[bolt], capture_target);
		}
	}
	capture_projectile = capture_bolts[0];
	platform_log("PLASMA shot tick %ld object %ld", capture_ticks, capture_projectile);
	if (capture_projectile != NONE)
	{
		struct object_datum *object = object_get(capture_projectile);
		struct object_definition *definition = object_definition_get(tag);
		struct object_attachment_definition *attachment =
			TAG_BLOCK_GET_ELEMENT(&definition->object.attachments, 0, struct object_attachment_definition);
		long *light = tag_get('ligh', attachment->type.index);
		platform_log("PLASMA tag %s type %d", tag_get_name(tag), object->object.type);
		platform_log("PLASMA enabled %d light flags %lx radius %.4f",
					 config_boolean("display.plasma_lighting"), light[0], ((real *)light)[1]);
		for (i = 0; i < MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT; i++)
			platform_log("PLASMA attachment %ld type %d index %ld", i, object->object.attachment_types[i],
						 object->object.attachment_indices[i]);
	}
}

void plasma_capture_frame(void)
{
	long index, selected = 0;
	real_matrix4x3 *matrix;
	if (capture_ticks < 85 || capture_ticks > 240)
		return;
	platform_log("PLASMA draws model %lu specular %lu", capture_model_draws, capture_specular_draws);
	capture_model_draws = capture_specular_draws = 0;
	for (index = 0; index < rasterizer_lights.light_count; index++)
		selected += plasma_lighting_selected(index);
	if (capture_projectile != NONE && object_try_and_get(capture_projectile))
	{
		matrix = object_get_node_matrix(capture_projectile, 0);
		platform_log("PLASMA frame %lu tick %ld fraction %.5f position %.5f %.5f %.5f selected %ld",
					 GetTickCount(), capture_ticks, render_interpolation_fraction(), matrix->position.x,
					 matrix->position.y, matrix->position.z, selected);
	}
	else
		platform_log("PLASMA frame %lu tick %ld no projectile selected %ld", GetTickCount(), capture_ticks,
					 selected);
}

int plasma_capture_should_capture(void)
{
	return capture_ticks >= 85 && capture_ticks <= 240 && capture_ticks % 60 >= 28 &&
		   capture_ticks % 60 <= 58;
}
