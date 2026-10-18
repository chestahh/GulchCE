/* Capture-only fixture, installed by shield_visual_build.py. */
#include "cseries.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "objects/damage.h"
#include "units/units.h"
#include "items/weapons.h"
#include "camera/observer.h"
#include "game/game.h"
#include "game/players.h"
#include "tag_files/tag_groups.h"
#include "cutscene/cinematics.h"
#include "ai/ai.h"
#include "shaders/shaders.h"
#include "shaders/shader_definitions.h"
#include "shield_look.h"
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
int config_write(char const *name, char const *value);
static long shield_capture_ticks, shield_capture_last_time;
static long shield_capture_target = NONE;
static long shield_capture_frame_tick = -1;

void shield_capture_tick(void)
{
	long now = game_time_get();
	long player, tag, index, weapon;
	struct object_placement_data placement;
	struct observer_result const *camera;
	struct damage_data damage;
	struct object_datum *object;
	if (now < shield_capture_last_time)
	{
		shield_capture_ticks = 0;
		shield_capture_target = NONE;
	}
	shield_capture_last_time = now;
	if (!game_in_progress() || cinematic_in_progress()) return;
	player = player_control_get_unit_index(0);
	if (player == NONE) return;
	ai_globals_ai_active(FALSE);
	shield_capture_ticks++;
	if (shield_capture_ticks == 60)
	{
		camera = observer_get_camera(0);
		tag = tag_loaded('weap', "weapons\\pistol\\pistol");
		if (tag != NONE)
		{
			unit_drop_current_weapon(player, TRUE);
			unit_drop_current_weapon(player, TRUE);
			object_placement_data_new(&placement, tag, NONE);
			placement.position = object_get(player)->object.position;
			weapon = object_new(&placement);
			if (weapon != NONE && unit_add_weapon_to_inventory(player, weapon, TRUE))
				for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
					if (unit_inventory_get_weapon(player, (short)index) == weapon)
						player_control_set_desired_weapon(player, (short)index);
		}
		tag = object_get(player)->definition_index;
		if (getenv("HALO_SHIELD_CAPTURE_ELITE"))
			tag = tag_loaded('bipd', "characters\\elite\\elite");
		if (tag != NONE && !getenv("HALO_SHIELD_CAPTURE_NO_TARGET"))
		{
			object_placement_data_new(&placement, tag, NONE);
			placement.position = camera->position;
			placement.position.x += camera->forward.i * 1.8f + camera->forward.j * 0.4f;
			placement.position.y += camera->forward.j * 1.8f - camera->forward.i * 0.4f;
			placement.position.z -= 0.6f;
			placement.forward = camera->forward;
			placement.forward.i = -placement.forward.i;
			placement.forward.j = -placement.forward.j;
			placement.forward.k = 0;
			normalize3d(&placement.forward);
			observer_up_from_forward(&placement.forward, &placement.up);
			shield_capture_target = object_new(&placement);
		}
		platform_log("SHIELD capture player %ld target %ld", player, shield_capture_target);
	}
	if (shield_capture_ticks == 90 || shield_capture_ticks == 120)
	{
		tag = tag_loaded('jpt!', "weapons\\assault rifle\\bullet");
		for (index = 0; index < 2; index++)
		{
			long target = index ? shield_capture_target : player;
			if (target == NONE) continue;
			if (shield_capture_ticks == 90)
			{
				struct object_datum *unit = object_get(target);
				struct object_definition *definition = object_definition_get(unit->definition_index);
				long modifier = definition->object.modifier_shader.index;
				struct shader *shader = modifier != NONE ? shader_definition_get(modifier) : NULL;
				platform_log("SHIELD target %ld max %.4f modifier %ld type %d eligible %d",
					target, unit->object.maximum_shield_vitality, modifier, shader ? shader->base.type : -1,
					shield_look_eligible(target, shader, unit->object.outgoing_function_values, FALSE));
			}
			if (shield_capture_ticks == 120)
			{
				object_get(target)->object.shield_vitality = 0.f;
				object_deplete_shield(target);
			}
			else if (tag != NONE)
			{
				damage_data_new(&damage, tag);
				damage.origin = object_get(target)->object.position;
				damage.epicenter = damage.origin;
				damage.direction = *global_forward3d;
				object_cause_damage(&damage, target, NONE, NONE, NONE, global_up3d);
			}
		}
	}
	if (getenv("HALO_SHIELD_CAPTURE_TOGGLE"))
	{
		if (shield_capture_ticks == 180) config_write("display.shield_look", "classic");
		if (shield_capture_ticks == 240) config_write("display.shield_look", "enhanced");
	}
	if (shield_capture_ticks % 15 == 0)
	{
		object = object_get(player);
		platform_log("SHIELD tick %ld vitality %.4f damage %.4f flags %u functions %.4f %.4f %.4f %.4f",
			shield_capture_ticks, object->object.shield_vitality,
			object->object.recent_shield_damage, object->object.damage_flags,
			object->object.outgoing_function_values[0], object->object.outgoing_function_values[1],
			object->object.outgoing_function_values[2], object->object.outgoing_function_values[3]);
	}
}

int shield_capture_should_capture(void)
{
	if (shield_capture_ticks >= 75 && shield_capture_ticks <= 450 &&
		shield_capture_ticks % 15 == 0 && shield_capture_frame_tick != shield_capture_ticks)
	{
		shield_capture_frame_tick = shield_capture_ticks;
		platform_log("SHIELD screenshot tick %ld", shield_capture_ticks);
		return 1;
	}
	return 0;
}
