/* Capture builds only: measure exact depth reuse opportunities before adding
   a production cache. Alpha-tested textures are conservatively never reusable. */
static void modern_shadow_reuse_probe(void)
{
	static struct
	{
		float light[4];
		unsigned char draws[SHADOW_DRAWS * sizeof(draws[0])];
		int count, samples, reusable;
	} history[MODERN_SHADOW_LIGHTS][2];
	int slot = (int)(map - maps), pass, draw;
	for (pass = 0; pass < 1 + map->view_map; pass++)
	{
		int count = 0, same = history[slot][pass].samples &&
			!memcmp(history[slot][pass].light, map->light, sizeof(map->light));
		for (draw = 0; draw < draw_count; draw++)
		{
			unsigned char *previous;
			if (!(draws[draw].views & (1 << pass))) continue;
			previous = history[slot][pass].draws + count * sizeof(draws[0]);
			if (draws[draw].alpha || count >= history[slot][pass].count ||
				memcmp(previous, &draws[draw], sizeof(draws[0]))) same = 0;
			memcpy(previous, &draws[draw], sizeof(draws[0]));
			count++;
		}
		if (count != history[slot][pass].count) same = 0;
		history[slot][pass].count = count;
		memcpy(history[slot][pass].light, map->light, sizeof(map->light));
		history[slot][pass].samples++;
		history[slot][pass].reusable += same;
		if (!(history[slot][pass].samples % 120))
			platform_log("SHADOW REUSE slot %d view %d reusable %d samples %d",
				slot, pass, history[slot][pass].reusable, history[slot][pass].samples);
	}
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
