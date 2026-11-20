/* Capture-only near-source cases. Grenades keep their ordinary collision,
   attachment, arming, detonation and deletion updates; no held-light hook. */
#include "muzzle_lighting.h"
#include "effects/particles.h"
static long near_grenade = NONE;

long near_capture_tick(void) { return capture_ticks; }

float near_capture_strafe(void)
{
    const char *kind = getenv("HALO_SHADOW_NEAR_SOURCE");
    struct object_datum *object = object_try_and_get(near_grenade);
    if (!kind || strcmp(kind, "stuck") || !object ||
        object->object.parent_object_index != shadow_subject || capture_ticks < 105 || capture_ticks > 148)
        return 0;
    return capture_ticks < 128 ? 0.35f : -0.35f;
}

static void near_source_tick(void)
{
	const char *kind = getenv("HALO_SHADOW_NEAR_SOURCE");
	struct object_datum *object;
	if (!kind) return;
	cheat.deathless_player = TRUE;
	if (!strcmp(kind, "muzzle"))
	{
		/* Exercise the actual render-time pulse envelope, including dark gaps.
		   This is an event/pose fixture, not a claim of player-input firing. */
		if (shadow_weapon != NONE && capture_ticks >= 100 && capture_ticks <= 190 && capture_ticks % 30 == 10)
			muzzle_lighting_shot(shadow_weapon, 0);
		return;
	}
	if (capture_ticks == 95)
	{
		real_point3d point = shadow_anchor;
		real_vector3d direction = shadow_forward;
		int stick = !strcmp(kind, "stuck");
		point.x += shadow_forward.i * 0.45f;
		point.y += shadow_forward.j * 0.45f;
		point.z += stick ? 0.4f : 0.25f;
		direction.i = -direction.i; direction.j = -direction.j;
		near_grenade = shadow_create('proj', "weapons\\plasma grenade\\plasma grenade", &point, &direction);
		if (near_grenade == NONE) { platform_log("NEAR failed grenade creation"); return; }
		object = object_get(near_grenade);
		object->object.translational_velocity.i = stick ? direction.i * 2.0f : 0;
		object->object.translational_velocity.j = stick ? direction.j * 2.0f : 0;
		object->object.translational_velocity.k = stick ? 0 : -0.3f;
		platform_log("NEAR created %s %ld at %.6f %.6f %.6f", kind, near_grenade, point.x, point.y, point.z);
	}
	if (near_grenade != NONE && capture_ticks % 5 == 0)
	{
		object = object_try_and_get(near_grenade);
		if (object)
		{
			struct projectile_datum *projectile = projectile_get(near_grenade);
			platform_log("NEAR grenade tick %ld id %ld parent %ld position %.6f %.6f %.6f arming %.6f timer %.6f flags %lu",
				capture_ticks, near_grenade, object->object.parent_object_index,
				object->object.position.x, object->object.position.y, object->object.position.z,
				projectile->projectile.arming_time, projectile->projectile.detonation_timer, projectile->projectile.flags);
			if (capture_ticks == 100 || capture_ticks == 115 || capture_ticks == 130 || capture_ticks == 145)
			{
				struct data_iterator particles;
				struct particle_datum *particle;
				struct object_marker marker;
				object_get_marker_by_name(near_grenade, "", &marker, 1);
				platform_log("NEAR model origin %.6f %.6f %.6f forward %.6f %.6f %.6f up %.6f %.6f %.6f",
					marker.matrix.position.x, marker.matrix.position.y, marker.matrix.position.z,
					marker.matrix.forward.i, marker.matrix.forward.j, marker.matrix.forward.k,
					marker.matrix.up.i, marker.matrix.up.j, marker.matrix.up.k);
				data_iterator_new(&particles, particle_data);
				while ((particle = data_iterator_next(&particles)) != NULL)
					platform_log("NEAR particle %s owner %ld node %d position %.6f %.6f %.6f radius %.6f color %.3f %.3f %.3f %.3f",
						tag_get_name(particle->definition_index), particle->object_index, particle->node_index,
						particle->position.x, particle->position.y, particle->position.z, particle->radius,
						particle->color.alpha, particle->color.red, particle->color.green, particle->color.blue);
			}
		}
		else { platform_log("NEAR removed tick %ld id %ld", capture_ticks, near_grenade); near_grenade = NONE; }
	}
}
