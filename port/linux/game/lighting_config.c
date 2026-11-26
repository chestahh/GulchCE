/* SPDX-FileCopyrightText: chestahh (GulchCE)
   SPDX-License-Identifier: AGPL-3.0-or-later OR GPL-3.0-or-later
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */

/* Avoid repeated locked, full-table configuration searches per light/draw.
   Cache values only until the settings generation changes; names and defaults
   remain owned by port_config.c. No map, object or host-policy state is cached. */
#include "lighting_config.h"
#include <string.h>

int config_boolean(const char *name);
unsigned long config_changes(void);

static struct
{
	const char *name;
	int value;
} preferences[] = {
	{"display.plasma_lighting", 0},
	{"display.muzzle_lighting", 0},
	{"display.plasma_occlusion", 0},
	{"display.ammo_counter_lighting", 0},
	{"display.needler_lighting", 0},
	{"display.jackal_shield_lighting", 0},
	{"display.covenant_shield_lighting", 0},
	{"display.fuel_rod_lighting", 0},
	{"display.weapon_glow", 0},
	{"display.vehicle_lighting", 0},
	{"display.world_lighting", 0}
};

int lighting_config_boolean(const char *name)
{
	static unsigned long changes;
	static int ready;
	unsigned int i;
	if (!ready || changes != config_changes())
	{
		/* A concurrent settings change must cause another refresh, rather
		   than label partially read preferences with the newer generation. */
		changes = config_changes();
		for (i = 0; i < sizeof(preferences) / sizeof(preferences[0]); i++)
			preferences[i].value = config_boolean(preferences[i].name);
		ready = 1;
	}
	for (i = 0; i < sizeof(preferences) / sizeof(preferences[0]); i++)
		if (!strcmp(name, preferences[i].name)) return preferences[i].value;
	return config_boolean(name);
}
