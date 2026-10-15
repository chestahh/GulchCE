/* Test-build-only fixture. The visual runner installs and removes this file.
			Creates real plasma projectiles; never linked into the normal game. */
#include "cseries.h"
#include <xtl.h>
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/damage.h"
#include "physics/collisions.h"
#include "items/projectiles.h"
#include "items/projectile_definitions.h"
#include "items/weapons.h"
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
#include "game/cheats.h"
#include "ai/ai.h"
#include <stdlib.h>
#include <string.h>

void platform_log(const char *format, ...);
int config_boolean(const char *name);
long light_port_glow_definition_get(void);
real render_interpolation_fraction(void);
static long capture_ticks, capture_projectile = NONE, capture_last_time;
static struct observer_result capture_camera;
static int capture_model, capture_probe, capture_burst = 1;
static long capture_bolts[32], capture_door = NONE;
static int capture_combat, capture_toggle;
static long capture_target = NONE;
static long capture_emitter = NONE;
static long capture_emitter_holder = NONE;
static int capture_emitter_attached;
static void ammo_capture_tick(const char *kind)
{
	struct object_placement_data placement;
	long tag, holder;
	int i;
	if (capture_ticks == 65)
	{
		holder = player_control_get_unit_index(0);
		if (strcmp(kind, "ammo-fp"))
		{
			tag = tag_loaded('bipd', !strcmp(kind, "ammo-marine") ? "characters\\marine_armored\\marine_armored" : "characters\\cyborg\\cyborg");
			if (tag == NONE) { platform_log("EMITTER missing actor"); return; }
			object_placement_data_new(&placement, tag, NONE);
			placement.position = capture_camera.position;
			placement.position.x += capture_camera.forward.i * 1.5f;
			placement.position.y += capture_camera.forward.j * 1.5f;
			placement.position.z -= 0.6f;
			placement.forward = capture_camera.forward;
			placement.forward.k = 0;
			normalize3d(&placement.forward);
			observer_up_from_forward(&placement.forward, &placement.up);
			holder = object_new(&placement);
		}
		tag = tag_loaded('weap', "weapons\\assault rifle\\assault rifle");
		if (holder == NONE || tag == NONE) return;
		capture_emitter_holder = holder;
		unit_drop_current_weapon(holder, TRUE);
		unit_drop_current_weapon(holder, TRUE);
		object_placement_data_new(&placement, tag, NONE);
		placement.position = object_get(holder)->object.position;
		capture_emitter = object_new(&placement);
		if (capture_emitter != NONE && unit_add_weapon_to_inventory(holder, capture_emitter, TRUE))
		{
			if (!strcmp(kind, "ammo-fp"))
				for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++)
					if (unit_inventory_get_weapon(holder, (short)i) == capture_emitter)
						player_control_set_desired_weapon(holder, (short)i);
			platform_log("EMITTER created %ld tag ammo holder %ld mode %s", capture_emitter, holder, kind);
		}
	}
	if (capture_ticks == 200 && capture_emitter_holder != NONE)
	{
		unit_drop_current_weapon(capture_emitter_holder, TRUE);
		platform_log("EMITTER ammo dropped");
	}
}
static void emitter_capture_tick(void)
{
	const char *kind = getenv("HALO_EMITTER_CAPTURE");
	struct object_placement_data placement;
	long tag;
	if (!strncmp(kind, "ammo-", 5))
	{
		ammo_capture_tick(kind);
		return;
	}
	if (capture_ticks == 65)
	{
		const char *name = !strcmp(kind, "stationary") ? "scenery\\c_field_generator\\c_field_generator" :
			!strcmp(kind, "fuel") ? "weapons\\fuel rod gun\\fuel rod" :
			!strcmp(kind, "major") ? "characters\\jackal\\jackal major" : "characters\\jackal\\jackal";
		capture_emitter_attached = FALSE;
		tag = tag_loaded(!strcmp(kind, "stationary") ? 'scen' : !strcmp(kind, "fuel") ? 'weap' : 'bipd', name);
		if (tag == NONE) { platform_log("EMITTER missing tag %s", name); return; }
		object_placement_data_new(&placement, tag, NONE);
		placement.position = capture_camera.position;
		placement.position.x += capture_camera.forward.i * 2.5f;
		placement.position.y += capture_camera.forward.j * 2.5f;
		placement.position.z -= 0.65f;
		if (!strcmp(kind, "stationary"))
		{
			struct collision_result ground;
			real_vector3d down = {0, 0, -5};
			real_point3d from = placement.position;
			from.z = capture_camera.position.z + 0.5f;
			if (collision_test_vector(FLAG(_collision_test_structure_bit) |
				FLAG(_collision_test_front_facing_surfaces_bit), &from, &down, NONE, &ground))
				placement.position.z = ground.point.z + 0.03f;
		}
		placement.forward = capture_camera.forward;
		placement.forward.i = -placement.forward.i;
		placement.forward.j = -placement.forward.j;
		placement.forward.k = 0;
		if (!strcmp(kind, "stationary"))
		{
			placement.forward.i = -capture_camera.forward.j;
			placement.forward.j = capture_camera.forward.i;
		}
		normalize3d(&placement.forward);
		observer_up_from_forward(&placement.forward, &placement.up);
		capture_emitter = object_new(&placement);
		platform_log("EMITTER created %ld tag %s", capture_emitter, name);
		if (!strcmp(kind, "fuel"))
		{
			tag = tag_loaded('bipd', "characters\\grunt\\grunt");
			if (tag != NONE)
			{
				struct object_placement_data holder;
				object_placement_data_new(&holder, tag, NONE);
				holder.position = placement.position;
				holder.forward = placement.forward;
				holder.up = placement.up;
				capture_emitter_holder = object_new(&holder);
				if (capture_emitter_holder != NONE && capture_emitter != NONE)
				{
					int held = unit_add_weapon_to_inventory(capture_emitter_holder, capture_emitter, TRUE);
					/* This fixture creates the gun loose before attaching it. Clear
					   that spawn-time detonation; the later stock drop stays live. */
					if (held) weapon_get(capture_emitter)->item.detonation_ticks = 0;
					platform_log("EMITTER held %d", held);
				}
			}
		}
	}
	if (capture_emitter != NONE && object_try_and_get(capture_emitter))
	{
		if (!strcmp(kind, "fuel") && !capture_emitter_attached &&
			object_get(capture_emitter)->object.parent_object_index == capture_emitter_holder)
		{
			/* Inventory insertion precedes the actual hand attachment. The
			   loose spawn can arm itself until that animation completes. */
			weapon_get(capture_emitter)->item.detonation_ticks = 0;
			capture_emitter_attached = TRUE;
			platform_log("EMITTER attached");
		}
		if (capture_ticks == 150 && strcmp(kind, "fuel"))
		{
			object_deplete_shield(capture_emitter);
			platform_log("EMITTER depleted %ld", capture_emitter);
		}
		if (capture_ticks == 180 && strcmp(kind, "fuel"))
		{
			struct damage_network_state state;
			damage_get_network_state(capture_emitter, &state);
			state.shield_depleted = FALSE;
			state.shield_vitality = 1.0f;
			damage_set_network_state(capture_emitter, &state);
			platform_log("EMITTER recovered %ld", capture_emitter);
		}
		if (capture_ticks == 150 && !strcmp(kind, "fuel") && capture_emitter_holder != NONE)
		{
			unit_drop_current_weapon(capture_emitter_holder, TRUE);
			platform_log("EMITTER dropped");
		}
		if (capture_ticks == 225)
		{
			object_delete(capture_emitter);
			capture_emitter = NONE;
			platform_log("EMITTER deleted");
		}
	}
}
static const char *capture_projectile_tags[] = {
	"weapons\\plasma rifle\\bolt", "weapons\\plasma pistol\\bolt",
	"weapons\\plasma rifle\\charged bolt", "weapons\\rocket launcher\\rocket",
	"weapons\\needler\\needle",
	"vehicles\\ghost\\ghost bolt",
	"vehicles\\banshee\\banshee bolt",
	"vehicles\\banshee\\banshee fuel rod",
	"vehicles\\c gun turret\\c gun turret",
	"weapons\\fuel rod gun\\fuel rod",
	"vehicles\\wraith\\wraith mortar",
	"weapons\\needler\\mp_needle",
	"vehicles\\banshee\\mp_banshee fuel rod",
	"vehicles\\c gun turret\\mp gun turret",
	"weapons\\plasma_cannon\\plasma_cannon"
};
static int capture_kind(void)
{
	const char *kind = getenv("HALO_PLASMA_CAPTURE_KIND");
	if (kind && !strcmp(kind, "pistol")) return 1;
	if (kind && !strcmp(kind, "charged")) return 2;
	if (kind && !strcmp(kind, "rocket")) return 3;
	if (kind && !strcmp(kind, "needle")) return 4;
	if (kind && !strcmp(kind, "ghost")) return 5;
	if (kind && !strcmp(kind, "banshee")) return 6;
	if (kind && !strcmp(kind, "banshee-fuel")) return 7;
	if (kind && !strcmp(kind, "shade")) return 8;
	if (kind && !strcmp(kind, "fuel")) return 9;
	if (kind && !strcmp(kind, "wraith")) return 10;
	if (kind && !strcmp(kind, "mp-needle")) return 11;
	if (kind && !strcmp(kind, "mp-banshee-fuel")) return 12;
	if (kind && !strcmp(kind, "mp-shade")) return 13;
	if (kind && !strcmp(kind, "ce-fuel")) return 14;
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
		capture_emitter = capture_emitter_holder = NONE;
		for (i = 0; i < 32; i++)
			capture_bolts[i] = NONE;
	}
	capture_last_time = now;
	if (!game_in_progress())
		return;
	if ((getenv("HALO_PLASMA_CAPTURE_DOOR") || getenv("HALO_COVENANT_CAPTURE_CAMPAIGN")) && cinematic_in_progress())
		return;
	/* Keep campaign captures reproducible without AI attacks or reloads from
	   the fixture's nearby explosions. Projectile flight/effects remain live. */
	if (getenv("HALO_COVENANT_CAPTURE_CAMPAIGN"))
	{
		ai_globals_ai_active(FALSE);
		cheat.deathless_player = TRUE;
	}
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
	if (getenv("HALO_EMITTER_CAPTURE"))
	{
		emitter_capture_tick();
		return;
	}
	if (capture_ticks == 65 && capture_combat)
	{
		long unit = player_control_get_unit_index(0);
		const char *role = getenv("HALO_NETWORK_TEST");
		if (getenv("HALO_WARTHOG_CAPTURE"))
		{
			long vehicle;
			if (role && !strncmp(role, "join", 4)) return;
			tag = tag_loaded('vehi', "vehicles\\warthog\\warthog");
			if (tag == NONE || unit == NONE) return;
			object_placement_data_new(&placement, tag, NONE);
			placement.position = capture_camera.position;
			placement.position.x += capture_camera.forward.i * 4.0f;
			placement.position.y += capture_camera.forward.j * 4.0f;
			placement.position.z -= 0.5f;
			placement.forward = capture_camera.forward;
			placement.forward.k = 0;
			normalize3d(&placement.forward);
			observer_up_from_forward(&placement.forward, &placement.up);
			vehicle = object_new(&placement);
			if (vehicle != NONE)
			{
				unit_scripting_enter_vehicle(unit, vehicle, "W-gunner");
				platform_log("WARTHOG seat vehicle %ld parent %ld", vehicle, object_get(unit)->object.parent_object_index);
				if (object_get(unit)->object.parent_object_index == vehicle)
					platform_log("PLASMA combat equipped Warthog %ld", vehicle);
			}
			return;
		}
		tag = tag_loaded('weap', capture_kind() == 3 ? "weapons\\rocket launcher\\rocket launcher" : capture_kind() ? "weapons\\plasma pistol\\plasma pistol" : "weapons\\plasma rifle\\plasma rifle");
		if (getenv("HALO_MUZZLE_CAPTURE_WEAPON"))
			tag = tag_loaded('weap', getenv("HALO_MUZZLE_CAPTURE_WEAPON"));
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
	placement.forward.k -= capture_option("HALO_PLASMA_CAPTURE_DROP", 0.12f);
	normalize3d(&placement.forward);
	observer_up_from_forward(&placement.forward, &placement.up);
	for (bolt = 0; bolt < capture_burst; bolt++)
	{
		placement.position.z += 0.025f;
		if (getenv("HALO_PLASMA_CAPTURE_MIXED"))
			placement.definition_index = tag_loaded(PROJECTILE_DEFINITION_TAG, capture_projectile_tags[bolt % (capture_kind() == 3 ? 4 : 3)]);
		capture_bolts[bolt] = object_new(&placement);
		if (capture_bolts[bolt] != NONE && capture_target != NONE && (capture_kind() == 2 || capture_kind() == 4 || capture_kind() == 11) && !capture_probe)
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
		platform_log("PLASMA tag %s type %d nodes %ld flags %lx", tag_get_name(tag), object->object.type, object->object.node_matrices.size, object->object.flags);
		if (capture_kind() == 3 || capture_kind() == 4 || capture_kind() == 10 || capture_kind() == 11)
		{
			long glow = light_port_glow_definition_get();
			if (glow != NONE)
			{
				long *light = tag_get('ligh', glow);
				platform_log("PLASMA glow template %s flags %lx", tag_get_name(glow), light[0]);
			}
		}
		for (i = 0; i < definition->object.attachments.count; i++)
		{
			struct object_attachment_definition *attachment =
				TAG_BLOCK_GET_ELEMENT(&definition->object.attachments, i, struct object_attachment_definition);
			if (attachment->type.group_tag == 'ligh' && attachment->type.index != NONE)
			{
				long *light = tag_get('ligh', attachment->type.index);
				platform_log("PLASMA enabled %d light flags %lx radius %.4f",
					config_boolean("display.plasma_lighting"), light[0], ((real *)light)[1]);
			}
		}
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
	return !getenv("HALO_PLASMA_CAPTURE_NO_IMAGES") && capture_ticks >= 85 && capture_ticks <= 240 && capture_ticks % 60 >= 28 &&
		   capture_ticks % 60 <= 58;
}
