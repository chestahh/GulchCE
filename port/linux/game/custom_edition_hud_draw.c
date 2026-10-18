/* CE draws without a converted placement. Adapted from ChupathingyCE
 * #352, by Boom Pen / ugoboom (GPL-3.0-or-later). See
 * docs/custom_edition_caches.md for provenance and the map-family boundary.
 */
#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "cache_file_formats.h"
#include "custom_edition_cache.h"
#include "custom_edition_hud_scale.h"
#include "custom_edition_hud_draw.h"

real custom_edition_hud_bitmap_scale(long bitmap_group_index)
{
	if (!custom_edition_cache_tags_loaded() || bitmap_group_index == NONE)
		return 1.0f;
	return custom_edition_hud_scale(bitmap_group_get(bitmap_group_index)->flags, FALSE);
}

real custom_edition_hud_number_scale(long bitmap_group_index, unsigned short placement_flags)
{
	boolean high_resolution;

	if (!custom_edition_cache_tags_loaded() || bitmap_group_index == NONE)
		return 1.0f;
	/* The known-map number fix already halves the digit metrics and sets
	the bitmap's half-scale flag. Its placement must not halve them again. */
	high_resolution = (placement_flags & 4) &&
		!custom_edition_cache_relies_on(_custom_edition_behaviour_hud_number_scale);
	return custom_edition_hud_scale(bitmap_group_get(bitmap_group_index)->flags, high_resolution);
}
