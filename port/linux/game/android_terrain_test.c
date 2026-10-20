/* Select the authored Extinction terrain material, not a transient GPU ID. */
#ifdef HALO_ANDROID
#include "cseries.h"
#include "tag_files/tag_files.h"
#include "custom_edition_cache.h"
#include "android_terrain_test.h"
#include <string.h>

void android_terrain_test_scope(int pass);

void android_terrain_test_begin(long base_bitmap, int pass)
{
	char const *name = NULL;

	if (custom_edition_cache_tags_loaded() && base_bitmap != NONE)
		name = tag_get_name(base_bitmap);
	android_terrain_test_scope(name &&
		!strcmp(name, "levels\\extinction\\bitmaps\\extinction_terrain2") ? pass : 0);
}

void android_terrain_test_end(void)
{
	android_terrain_test_scope(0);
}
#endif
