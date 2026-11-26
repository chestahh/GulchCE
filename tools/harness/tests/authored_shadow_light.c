#include "harness.h"

enum { _point_light_dynamic_bit=0, _point_light_port_glow_bit=4,
       _light_definition_dynamic_bit=0, _light_definition_is_first_person_flashlight_bit=4 };
struct light_datum { unsigned flags; long definition_index, parent_light_index; };
struct point_light_definition { unsigned flags; };
static struct light_datum light;
static struct point_light_definition definition;
static struct pool { int valid; } pool, *light_data = &pool;
static void *datum_try_and_get(struct pool *storage, long id) { return id == 77 ? &light : NULL; }
static struct point_light_definition *light_definition_get(long id) { return &definition; }
#include "under_test.inc"

int main(void)
{
    pool.valid = TRUE;
    light.flags = FLAG(_point_light_dynamic_bit);
    light.parent_light_index = NONE;
    definition.flags = FLAG(_light_definition_dynamic_bit);
    CHECK(light_port_authored_dynamic(77),"native dynamic attachments qualify independently of owner class");
    light.flags |= FLAG(_point_light_port_glow_bit);
    CHECK(!light_port_authored_dynamic(77),"synthetic sources keep their own category settings");
    light.flags = FLAG(_point_light_dynamic_bit);
    definition.flags = 0;
    CHECK(!light_port_authored_dynamic(77),"temporarily promoted flare is not an authored dynamic attachment");
    light.parent_light_index = 0;
    CHECK(light_port_authored_dynamic(77),"an actual effect light qualifies even with an unflagged definition");
    definition.flags = FLAG(_light_definition_is_first_person_flashlight_bit);
    CHECK(!light_port_authored_dynamic(77),"flashlight keeps one independent projector");
    definition.flags = 0; light.flags = 0;
    CHECK(!light_port_authored_dynamic(77),"inactive light cannot consume a slot");
    CHECK(!light_port_authored_dynamic(78) && !light_port_authored_dynamic(NONE),"stale handles rejected");
    pool.valid = FALSE;
    CHECK(!light_port_authored_dynamic(77),"unloaded map has no sources");
    return 0;
}
