/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */

/* Reuse authored attachment timing, colours, cones and render poses. Never
   add a second light beside an existing source or change its simulation. */
#include "cseries.h"
#include "objects/objects.h"
#include "tag_files/tag_files.h"
#include "authored_emitter_lighting.h"

#include "lighting_config.h"

static const struct authored_emitter_profile
{
	long group;
	const char *object, *light;
	float scale;
	int category;
	float forward_offset;
	int owner_unshadowed;
} profiles[] = {
	{'weap', "weapons\\energy sword\\energy sword", "weapons\\energy sword\\energy sword dynamic", 2.5f, 1},
	{'proj', "weapons\\plasma grenade\\plasma grenade", "weapons\\plasma grenade\\plasma grenade", 1.5f, AUTHORED_EMITTER_PLASMA_GRENADE},
	{'weap', "weapons\\plasma pistol\\plasma pistol", "weapons\\plasma rifle\\overcharge", 1.5f, 1},
	{'vehi', "vehicles\\ghost\\ghost", "vehicles\\ghost\\ghost running", 2.2f, 2, 0.02f, TRUE},
	{'vehi', "vehicles\\banshee\\banshee", "vehicles\\banshee\\banshee wing", 2.2f, AUTHORED_EMITTER_BANSHEE_WING, 0.f, TRUE},
	{'vehi', "vehicles\\banshee\\banshee_mp", "vehicles\\banshee\\banshee wing", 2.2f, AUTHORED_EMITTER_BANSHEE_WING, 0.f, TRUE},
	{'vehi', "vehicles\\warthog\\mp_warthog", "vehicles\\warthog\\headlights light", 1.0f, 2},
	{'vehi', "vehicles\\rwarthog\\rwarthog", "vehicles\\warthog\\headlights light", 1.0f, 2},
	{'vehi', "vehicles\\scorpion\\scorpion_mp", "vehicles\\scorpion\\headlight scorpion", 1.0f, 2},
	{'vehi', "vehicles\\warthog\\warthog", "vehicles\\warthog\\headlights light", 1.0f, 2},
	{'vehi', "vehicles\\scorpion\\scorpion", "vehicles\\scorpion\\headlight scorpion", 1.0f, 2},
};
static struct { long object, light; } identities[NUMBEROF(profiles)];
static int identities_ready;

int authored_emitter_lighting_category_enabled(int category)
{
	return lighting_config_boolean(category == 1 ? "display.weapon_glow" :
		(category == 2 || category == AUTHORED_EMITTER_BANSHEE_WING) ? "display.vehicle_lighting" : "display.plasma_lighting");
}

int authored_emitter_lighting_enabled(void)
{
	return lighting_config_boolean("display.weapon_glow") || lighting_config_boolean("display.vehicle_lighting");
}

/* Both authored attachments use marker zero on the same wing. Place the
   rear light on the opposite wing when the model supplies the paired markers. */
void authored_emitter_wing_position(long light, long index, real_point3d *position)
{
	struct object_datum const *object = object_get(index);
	struct object_marker front, back[2];
	int i;
	for (i = 0; i < MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT; i++)
	{
		const char *name;
		if (object->object.attachment_types[i] != _object_attachment_type_light ||
			object->object.attachment_indices[i] != light) continue;
		name = object_get_attachment_marker_name(index, i);
		if (name && !strcmp(name, "winglight back") &&
			object_get_marker_by_name(index, "winglight front", &front, 1) == 1 &&
			object_get_marker_by_name(index, name, back, 2) == 2 &&
			distance_squared3d(&front.matrix.position, &back[1].matrix.position) >
			distance_squared3d(&front.matrix.position, &back[0].matrix.position))
			*position = back[1].matrix.position;
		break;
	}
}

void authored_emitter_lighting_forget(void)
{
	identities_ready = FALSE;
}

int authored_emitter_lighting_profile(long object_index, long definition, float *scale, float *forward_offset, int *owner_unshadowed)
{
	struct object_datum *object;
	int i;
	*owner_unshadowed = FALSE;
	if (!authored_emitter_lighting_enabled() && !lighting_config_boolean("display.plasma_lighting"))
		return 0;
	object = object_try_and_get(object_index);
	if (!object || definition == NONE)
		return 0;
	if (!identities_ready)
	{
		for (i = 0; i < NUMBEROF(profiles); i++)
		{
			identities[i].object = tag_loaded(profiles[i].group, profiles[i].object);
			identities[i].light = tag_loaded('ligh', profiles[i].light);
		}
		identities_ready = TRUE;
	}
	for (i = 0; i < NUMBEROF(profiles); i++)
		if (identities[i].object != NONE && object->definition_index == identities[i].object &&
			definition == identities[i].light && authored_emitter_lighting_category_enabled(profiles[i].category))
		{
			*scale = profiles[i].scale;
			*forward_offset = profiles[i].forward_offset;
			*owner_unshadowed = profiles[i].owner_unshadowed;
			return profiles[i].category;
		}
	return 0;
}
