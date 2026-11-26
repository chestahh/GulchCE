#include "harness.h"
#include <string.h>
#define MAXIMUM_BREAKABLE_SURFACES_PER_MAP 256
struct structure_bsp { struct { int count; } breakable_surfaces; };
static unsigned char flags[32];
static int invalidated;
static byte *breakable_surface_flags_get(void) { return flags; }
static void modern_environment_invalidate_static(void) { invalidated++; }
#include "under_test.inc"

int main(void)
{
	struct structure_bsp bsp = {{200}};
	memset(flags, 255, sizeof(flags));
	check_static_breakables(&bsp);
	check_static_breakables(&bsp);
	CHECK(invalidated == 1, "unchanged glass retains cached static images");
	flags[24] ^= 8;
	check_static_breakables(&bsp);
	CHECK(invalidated == 2, "a broken pane invalidates sun and both fixtures");
	flags[24] ^= 8;
	check_static_breakables(&bsp);
	CHECK(invalidated == 3, "restored glass is included on the next view");
	return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
