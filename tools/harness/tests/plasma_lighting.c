#include "harness.h"
#include <string.h>

enum { _object_type_projectile = 5 };
enum { _point_light_dynamic_bit = 0 };
#define SET_FLAG(value, bit, enabled) ((value) = (enabled) ? ((value) | (1 << (bit))) : ((value) & ~(1 << (bit))))
struct object_datum { long definition_index; struct { short type; } object; };
static struct object_datum projectile = { 1, { _object_type_projectile } };
static const char *tag_name = "weapons\\plasma rifle\\bolt";
static int enabled = 1, refreshes, present = 1, interpolation_rendering = 1;
static int rendered_pose;
int config_boolean(const char *name) { return enabled; }
static struct object_datum *object_try_and_get(long index) { return present ? &projectile : NULL; }
static const char *tag_get_name(long index) { return tag_name; }
struct light_datum { long object_index, parent_light_index; int flags; };
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
int plasma_lighting_selected(long index);
void plasma_falloff_dispose(void) {}
void plasma_falloff_end(void) {}
#include "under_test.inc"

int main(int argc, char **argv)
{
	int i;
	const char *test = argv[1];

	if (!strcmp(test, "model"))
	{
		plasma_lighting_prepare(1,1); plasma_lighting_submitted(1,3);
		plasma_model_lighting_slot(3,0); plasma_model_lighting_slot(7,1);
		CHECK(plasma_model_lighting_mask()==1,"only selected slot is softened");
		plasma_model_lighting_slot(3,1);
		CHECK(plasma_model_lighting_mask()==3,"both selected slots supported");
		plasma_model_lighting_slot(NONE,0);
		CHECK(plasma_model_lighting_mask()==2,"empty slot clears previous model");
		enabled=0; CHECK(!plasma_model_lighting_mask(),"disable immediately gates model state");
		enabled=1; plasma_lighting_window_begin();
		CHECK(!plasma_model_lighting_mask(),"next view clears slots");
		plasma_lighting_submitted(1,3); plasma_model_lighting_slot(3,1);
		plasma_lighting_frame_end();
		CHECK(!plasma_model_lighting_mask(),"frame cleanup clears slots");
	}
	else if (!strcmp(test, "off"))
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
		CHECK(datum_try_and_get(light_data, 1)->flags == 1, "lens-flare attachment promoted only for rendering");
		enabled = 0;
		render_interpolation_frame_end();
		CHECK(!rendered_pose && refreshes == 2, "disable still restores simulation pose");
		CHECK(datum_try_and_get(light_data, 1)->flags == 0, "original dynamic flag restored");
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
	else if (!strcmp(test, "selection"))
	{
		plasma_lighting_prepare(1, 1);
		plasma_lighting_submitted(1, 3);
		CHECK(plasma_lighting_selected(3) && !plasma_lighting_selected(2), "only submitted plasma lights selected");
		CHECK(!plasma_lighting_selected(NONE), "failed submission excluded");
		plasma_lighting_window_begin();
		CHECK(!plasma_lighting_selected(3), "view indices cannot leak across split screen");
		plasma_lighting_submitted(1, 0);
		enabled = 0;
		CHECK(!plasma_lighting_selected(0), "disable immediately restores stock selection");
	}
	else if (!strcmp(test, "existing"))
	{
		datum_try_and_get(light_data, 1)->flags = 1;
		plasma_lighting_prepare(1, 1);
		render_interpolation_frame_end();
		CHECK(datum_try_and_get(light_data, 1)->flags == 1, "authored dynamic lighting stays dynamic");
	}
	return 0;
}
