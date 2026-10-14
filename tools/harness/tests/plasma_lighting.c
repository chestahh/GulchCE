#include "harness.h"
#include <string.h>

enum { _object_type_projectile = 5 };
struct object_datum { long definition_index; struct { short type; } object; };
static struct object_datum projectile = { 1, { _object_type_projectile } };
static const char *tag_name = "weapons\\plasma rifle\\bolt";
static int enabled = 1, refreshes, present = 1, interpolation_rendering = 1;
static int rendered_pose;
int config_boolean(const char *name) { return enabled; }
static struct object_datum *object_try_and_get(long index) { return present ? &projectile : NULL; }
static const char *tag_get_name(long index) { return tag_name; }
struct light_datum { long object_index, parent_light_index; };
static struct { int valid; } storage = { 1 }, *light_data = &storage;
static int stale;
static struct light_datum *datum_try_and_get(void *data, long index)
{
	static struct light_datum light;
	light.object_index = index + stale;
	light.parent_light_index = NONE;
	return &light;
}
static void light_disconnect_from_map(long index) {}
static void light_reconnect_to_map(long index)
{
	refreshes++;
	rendered_pose = interpolation_rendering;
}
void plasma_lighting_reset(void);
#include "under_test.inc"

int main(int argc, char **argv)
{
	int i;
	const char *test = argv[1];

	if (!strcmp(test, "off"))
	{
		enabled = 0;
		plasma_lighting_prepare(1, 1);
		CHECK(!refreshes, "disabled changes no attachments");
	}
	else if (!strcmp(test, "filter"))
	{
		tag_name = "weapons\\rocket launcher\\rocket";
		plasma_lighting_prepare(1, 1);
		tag_name = "weapons\\plasma rifle\\bolt";
		projectile.object.type = 0;
		plasma_lighting_prepare(1, 1);
		CHECK(!refreshes, "other projectile tags and object types stay stock");
	}
	else if (!strcmp(test, "restore"))
	{
		plasma_lighting_prepare(1, 1);
		CHECK(rendered_pose && refreshes == 1, "draw uses render pose");
		enabled = 0;
		render_interpolation_frame_end();
		CHECK(!rendered_pose && refreshes == 2, "disable still restores simulation pose");
		plasma_lighting_frame_end();
		CHECK(refreshes == 2, "restoration happens once");
	}
	else if (!strcmp(test, "split"))
	{
		plasma_lighting_prepare(1, 1);
		plasma_lighting_prepare(1, 1);
		CHECK(refreshes == 1, "two views share one attachment refresh");
	}
	else if (!strcmp(test, "capacity"))
	{
		for (i = 0; i < 130; i++) plasma_lighting_prepare(i, i);
		CHECK(refreshes == 128, "overflow leaves excess lights untouched");
		render_interpolation_frame_end();
		CHECK(refreshes == 256 && !rendered_pose, "every accepted light restored");
	}
	else if (!strcmp(test, "unload"))
	{
		plasma_lighting_prepare(1, 1);
		plasma_lighting_reset();
		render_interpolation_frame_end();
		CHECK(refreshes == 1, "old map handles never revisited");
	}
	else if (!strcmp(test, "missing"))
	{
		present = 0;
		plasma_lighting_prepare(1, 1);
		plasma_lighting_frame_end();
		CHECK(!refreshes, "deleted projectile cannot acquire a render light");
	}
	else if (!strcmp(test, "stale"))
	{
		plasma_lighting_prepare(1, 1);
		stale = 1;
		render_interpolation_frame_end();
		CHECK(refreshes == 1, "changed light owner is not restored");
		storage.valid = 0;
		plasma_lighting_prepare(2, 2);
		CHECK(refreshes == 1, "invalid light storage is untouched");
	}
	return 0;
}
