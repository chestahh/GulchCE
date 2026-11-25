#include "harness.h"
#include "../../../port/linux/game/authored_emitter_lighting.h"
#include <string.h>
#define NUMBEROF(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define PIN(a,b,c) MIN(MAX(a,b),c)



enum { MAXIMUM_NUMBER_OF_ATTACHMENTS_PER_OBJECT=2, _object_attachment_type_light=0 };
struct object_marker { struct {real_point3d position;} matrix; };
struct object_datum { long definition_index; struct {real_point3d bounding_sphere_center; int attachment_types[2]; long attachment_indices[2];} object; } object;
struct observer_result {real_point3d position;} cameras[4];
static int local_count=1;
int local_player_count(void) {return local_count;}
struct observer_result const *observer_get_camera(short i) {return &cameras[i];}
static int weapon_on, vehicle_on, projectile_on, lookups, generation;
static const char *names[64];
static long groups[64];
static int name_count;
int lighting_config_boolean(const char *name)
{
	return !strcmp(name, "display.weapon_glow") ? weapon_on :
		!strcmp(name, "display.vehicle_lighting") ? vehicle_on : projectile_on;
}
struct object_datum *object_try_and_get(long index) { return index == 1 ? &object : NULL; }
long tag_loaded(long group, const char *name)
{
	int i;
	lookups++;
	for (i = 0; i < name_count; i++)
		if (group == groups[i] && !strcmp(name, names[i])) return generation + i;
	names[name_count] = name;
	groups[name_count] = group;
	return generation + name_count++;
}
static struct object_datum *object_get(long index) { return &object; }
static const char *object_get_attachment_marker_name(long index, int slot) { return slot ? "winglight back" : "winglight front"; }
static int object_get_marker_by_name(long index,const char *name,struct object_marker *markers,int count)
{
	markers[0].matrix.position.y=-1;
	if(count==2) markers[1].matrix.position.y=1;
	return count;
}
static float distance_squared3d(const real_point3d *a,const real_point3d *b) { return (a->y-b->y)*(a->y-b->y); }
#include "under_test.inc"
int main(int argc, char **argv)
{
	int i, calls, owner_unshadowed=7;
	float scale = 42, offset = 7;
	real_point3d position={0,-1,0};
	object.object.attachment_indices[0]=7;
	object.object.attachment_indices[1]=8;
	authored_emitter_wing_position(7,1,&position);
	CHECK(position.y==-1,"front light follows its original marker");
	authored_emitter_wing_position(8,1,&position);
	CHECK(position.y==1,"rear light follows the other wing");
	(void)argc; (void)argv;
	CHECK(!authored_emitter_lighting_profile(1, 10, &scale, &offset, &owner_unshadowed) && !lookups && scale == 42 && offset == 7 && !owner_unshadowed,
		"off performs no tag discovery or writes");
	for (i = 0; i < NUMBEROF(profiles); i++)
	{
		long light;
		weapon_on = profiles[i].category == 1;
		vehicle_on = profiles[i].category == 2 || profiles[i].category == AUTHORED_EMITTER_BANSHEE_WING;
		projectile_on = profiles[i].category == 3;
		object.definition_index = tag_loaded(profiles[i].group, profiles[i].object);
		light = tag_loaded('ligh', profiles[i].light);
		CHECK(authored_emitter_lighting_profile(1, light, &scale, &offset, &owner_unshadowed) == profiles[i].category &&
			scale == profiles[i].scale, "selected source uses its category and scale");
		CHECK(offset == (!strcmp(profiles[i].object, "vehicles\\ghost\\ghost") ? .02f : 0.f),
            "only the Ghost emitting surface changes position");
        CHECK(owner_unshadowed == (!strcmp(profiles[i].object, "vehicles\\ghost\\ghost") || profiles[i].category == AUTHORED_EMITTER_BANSHEE_WING),
            "Ghost and Banshee engine lights exclude their owner");
        calls = lookups;
		CHECK(authored_emitter_lighting_profile(1, light, &scale, &offset, &owner_unshadowed) && lookups == calls,
			"steady frames reuse cached identities");
		CHECK(!authored_emitter_lighting_profile(1, 60000, &scale, &offset, &owner_unshadowed), "unrelated light stays stock");
		CHECK(!authored_emitter_lighting_profile(2, light, &scale, &offset, &owner_unshadowed), "deleted owner stays stock");
		weapon_on = vehicle_on = projectile_on = 0;
		CHECK(!authored_emitter_lighting_profile(1, light, &scale, &offset, &owner_unshadowed), "disable immediately restores stock");
		CHECK(!owner_unshadowed,"disabled profile clears owner exception");
	}
	weapon_on = 1;
	generation = 1000;
	authored_emitter_lighting_forget();
	object.definition_index = tag_loaded('weap', "weapons\\energy sword\\energy sword");
	CHECK(authored_emitter_lighting_profile(1, tag_loaded('ligh', "weapons\\energy sword\\energy sword dynamic"), &scale, &offset, &owner_unshadowed),
		"new map resolves new tag identities");
	CHECK(!authored_emitter_lighting_profile(1, 1, &scale, &offset, &owner_unshadowed), "old map identity cannot match");
	return 0;
}
