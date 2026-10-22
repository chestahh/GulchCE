/* The whole feature module, with only engine services and cache I/O stubbed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define MAXIMUM_LOCAL_PLAYERS 4
#define MAXIMUM_SHADERS_PER_MODEL 32
#define NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS 4
#define TAG_STRING_LENGTH 31
#define _object_type_biped 0
#define _shader_type_transparent_plasma_glow 10
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef unsigned char boolean;
typedef unsigned char byte;
typedef unsigned short word;
typedef float real;
typedef struct { real red, green, blue; } real_rgb_color;
typedef struct { real i, j, k; } real_vector3d;
struct tag_reference { unsigned long group_tag; char *name; long name_length, index; };
struct tag_block { long count; void *address; void *definition; };
void shield_recharge_reset(void) {} /* separate draw-envelope dependency */
#include "real_types.inc"
struct object_definition { struct {
    struct tag_reference model, modifier_shader;
    short function_modes[4]; struct tag_block functions;
} object; };
struct object_datum { long definition_index; struct { short type; } object; };
struct unit_datum { struct { real outgoing_function_values[4]; real_rgb_color outgoing_change_colors[4]; } object; };
struct model { struct tag_block shaders; };
struct bitmap_data { short width, height, depth, type, format, mipmap_count; word flags; long pixels_size, pixels_offset; };
struct bitmap_group { struct tag_block bitmaps; };
struct game_globals_first_person_interface { struct tag_reference hands; };
struct game_globals { struct tag_block first_person_interface; };
struct render_model_effect { struct shader *modifier_shader; struct {
    real_rgb_color const *colors; real const *values;
} modifier_animation; };
#define TAG_BLOCK_GET_ELEMENT(block, i, type) ((type *)(block)->address + (i))

static const char *setting = "enhanced";
static void *tags[16];
static unsigned long groups[16];
static struct object_datum object;
static struct unit_datum unit;
static struct game_globals globals;
static int noise_matches = 1;
static int no_noise;
struct tag_iterator { long index; };
static void tag_iterator_new(struct tag_iterator *it, unsigned long group) { (void)group; it->index = 0; }
static long tag_iterator_next(struct tag_iterator *it) { return no_noise ? NONE : it->index++ == 0 ? 3 : it->index == 2 ? 9 : NONE; }
static int overshield;
static unsigned char *reference_pixels;
static long reference_size;
static int read_failed;
static const char *config_string(const char *name) { (void)name; return setting; }
static boolean cache_file_tag_cache_contains(void const *p, long size) { return p && size > 0; }
static unsigned long tag_get_group_tag(long i) { return i >= 0 && i < 16 ? groups[i] : 0; }
static void *bitmap_group_get(long i) { return tags[i]; }
static void *object_definition_get(long i) { return tags[i]; }
static boolean shader_type_is_valid_for_modifier(short type) { return type == 1 || (type > 4 && type <= 10); }
static void *shader_definition_get(long i) { return tags[i]; }
static void *model_definition_get(long i) { return tags[i]; }
static struct object_datum *object_get(long i) { (void)i; return &object; }
static struct object_datum *object_try_and_get(long i) { return i == NONE ? NULL : &object; }
static struct unit_datum *unit_get(long i) { (void)i; return &unit; }
static struct game_globals *scenario_get_game_globals(void) { return &globals; }
static int SwitchToThread(void) { return 0; }
static void cache_file_read(long tag, long offset, long size, void *out, boolean *complete, boolean blocking)
{
    (void)tag; (void)offset; (void)blocking;
    if (reference_pixels && reference_size >= size && !read_failed) {
        memcpy(out, reference_pixels, size); *complete = TRUE;
    } else *complete = 2;
}
static boolean overshield_effect_arms_values(long i, real const *source, real *out)
{
    (void)i;
    if (!overshield) return FALSE;
    memcpy(out, source, 4 * sizeof(real)); out[1] = 0.75f; return TRUE;
}
#include "under_test.inc"

static struct shield_look_plasma stock;
static struct object_definition definition;
static struct object_function_definition functions[2];
static struct shield_look_model_shader material;
static struct model model;
static struct model hands_model;
static struct shield_look_model_shader hands_material;
static struct { struct shader shader; word flags; } opaque;
static struct game_globals_first_person_interface first_person;
static struct bitmap_group bitmap_group;
static struct bitmap_data bitmap;

static void setup(void)
{
    stock.shader.base.type = 10;
    stock.intensity_source = stock.offset_source = 2;
    stock.intensity_exponent = 1.f; stock.offset_exponent = 2.f; stock.offset_amount = 0.03f;
    stock.perpendicular_alpha = stock.parallel_alpha = 1.f;
    stock.perpendicular_color.red = 1.f; stock.perpendicular_color.green = 0.545098f;
    stock.perpendicular_color.blue = 0.243137f;
    stock.parallel_color.red = stock.parallel_color.green = 0.0666667f; stock.parallel_color.blue = 1.f;
    stock.primary_period = 2.2f; stock.secondary_period = 3.f;
    stock.primary_direction.i = 2.f; stock.primary_direction.k = -0.3f;
    stock.secondary_direction.j = 2.f; stock.secondary_direction.k = 0.5f;
    stock.primary_scale = 10.f; stock.secondary_scale = 6.f;
    stock.reserved82[0x3B] = 6; stock.reservedE4[0x21] = 6;
    stock.primary_map.index = stock.secondary_map.index = 3;
    stock.primary_map.group_tag = stock.secondary_map.group_tag = 'bitm';
    functions[1].period = 1.f; functions[1].scale_function_by_function_index = 1;
    functions[1].map_result_to_transition_function = 2;
    functions[1].upper_bound = 1.f; functions[1].turn_off_with_function_index = NONE;
    functions[1].runtime_reciprocal_bounds_range = functions[1].runtime_one_over_period = 1.f;
    definition.object.function_modes[0] = 4; definition.object.function_modes[1] = 2;
    definition.object.functions.count = 2; definition.object.functions.address = functions;
    definition.object.modifier_shader.index = 2; definition.object.model.index = 4;
    model.shaders.count = 1; model.shaders.address = &material; material.shader.index = 5;
    hands_model.shaders.count = 1; hands_model.shaders.address = &hands_material; hands_material.shader.index = 8;
    first_person.hands.index = 7;
    globals.first_person_interface.count = 1; globals.first_person_interface.address = &first_person;
    tags[1] = &definition; tags[2] = &stock; tags[3] = &bitmap_group; tags[4] = &model;
    tags[5] = tags[8] = &opaque; tags[7] = &hands_model;
    groups[2] = 'spla'; groups[3] = 'bitm'; groups[5] = groups[8] = 'soso';
    object.definition_index = 1;
    unit.object.outgoing_function_values[1] = 0.5f;
    bitmap_group.bitmaps.count = 1; bitmap_group.bitmaps.address = &bitmap;
    bitmap.width = bitmap.height = bitmap.depth = 32; bitmap.type = 1; bitmap.format = 3;
    bitmap.mipmap_count = 5; bitmap.pixels_size = 75008; bitmap.flags = 0x89;
}

/* Simulate the caller's Arm Shields pass; profile code must preserve its
   shader-animation state, including the pointer used by the renderer cap. */
static int arm_setting = 1;
static real arm_values[4][4];
static boolean draw_arms(short local, long unit_index, long hands, struct render_model_effect *effect)
{
    memset(effect, 0, sizeof(*effect));
    if (arm_setting && local >= 0 && local < 4) {
        if (!overshield_effect_arms_values(unit_index, unit.object.outgoing_function_values, arm_values[local]))
            memcpy(arm_values[local], unit.object.outgoing_function_values, sizeof(arm_values[local]));
        effect->modifier_shader = &stock.shader;
        effect->modifier_animation.values = arm_values[local];
        effect->modifier_animation.colors = unit.object.outgoing_change_colors;
    }
    return shield_look_arms(local, unit_index, hands, effect);
}

static void export_shader(struct shield_look_plasma const *p)
{
    unsigned char const *b = (unsigned char const *)p;
    int spans[][2] = {{0x2C,0x40},{0x60,0x82},{0xC0,0xD4},{0x108,0x11C}};
    int s, i;
    for (s = 0; s < 4; s++) for (i = spans[s][0]; i < spans[s][1]; i++) printf("%02x", b[i]);
}

int main(int argc, char **argv)
{
    struct render_model_effect effect = {0}, second = {0};
    struct shader *body;
    int i;
    FILE *file;
    CHECK(argc >= 2);
    setup();
    if (argc == 3) setting = argv[2];
    if (!strcmp(argv[1], "reference")) {
        CHECK(argc == 4 || argc == 5); file = fopen(argv[2], "rb"); CHECK(file);
        CHECK(fread(&stock, 1, sizeof(stock), file) == sizeof(stock)); fclose(file);
        file = fopen(argv[3], "rb"); CHECK(file); fseek(file, 0, SEEK_END); reference_size = ftell(file);
        rewind(file); reference_pixels = malloc(reference_size); CHECK(reference_pixels);
        CHECK(fread(reference_pixels, 1, reference_size, file) == (size_t)reference_size); fclose(file);
        if (argc == 5) {
            bitmap.format = 11; bitmap.pixels_size = reference_size;
            bitmap.flags = !strcmp(argv[4], "ce") ? 0x181 : 0x189;
        }
        CHECK(shield_look_read_noise(3)); reference_pixels[0] ^= 1; CHECK(!shield_look_read_noise(3));
        reference_pixels[0] ^= 1; read_failed = 1; CHECK(!shield_look_read_noise(3)); free(reference_pixels);
        return 0;
    }
    if (!strcmp(argv[1], "honeycomb")) {
        struct shader *enhanced, *honeycomb, *arms;
        struct shield_look_plasma saved;
        enhanced = shield_look_body(0, &stock.shader);
        saved = *(struct shield_look_plasma *)enhanced;
        CHECK(!shield_look_honeycomb_scale(enhanced));
        setting = "honeycomb";
        honeycomb = shield_look_body(0, &stock.shader);
        CHECK(honeycomb != enhanced && shield_look_honeycomb_scale(honeycomb) == 87.5f);
        CHECK(!memcmp(honeycomb, &saved, sizeof(saved)));
        CHECK(draw_arms(0, 0, 7, &effect));
        arms = effect.modifier_shader;
        CHECK(shield_look_honeycomb_scale(arms) == 225.f);
        CHECK(effect.modifier_animation.values == arm_values[0]);
        arm_setting = 0; CHECK(!draw_arms(0, 0, 7, &effect));
        CHECK(!effect.modifier_shader);
        setting = "classic_with_sprinkles";
        CHECK(!shield_look_honeycomb_scale(shield_look_body(0, &stock.shader)));
        CHECK(shield_look_honeycomb_scale(honeycomb));
        setting = "enhanced";
        CHECK(shield_look_body(0, &stock.shader) == enhanced);
        CHECK(!memcmp(enhanced, &saved, sizeof(saved)));
        setting = "classic";
        CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(!shield_look_honeycomb_scale(&stock.shader));
        shield_look_reset(); CHECK(!shield_look_honeycomb_scale(honeycomb));
        return 0;
    }
    if (!strcmp(argv[1], "sprinkles")) {
        struct shield_look_plasma enhanced, saved, *p;
        struct shader *previous;
        real_rgb_color a, b;
        previous = shield_look_body(0, &stock.shader);
        enhanced = *(struct shield_look_plasma *)previous;
        setting = "classic_with_sprinkles";
        body = shield_look_body(0, &stock.shader); p = (struct shield_look_plasma *)body;
        CHECK(body != previous && body != &stock.shader);
        CHECK(p->primary_scale == 10.f && p->secondary_scale == 6.f);
        CHECK(p->primary_period == 2.2f && p->secondary_period == 3.f);
        CHECK(p->perpendicular_color.red == 1.f && p->parallel_color.blue == 1.f);
        CHECK(p->offset_amount == enhanced.offset_amount && p->offset_exponent == enhanced.offset_exponent);
        CHECK(draw_arms(0, 0, 7, &effect));
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->primary_scale == 10.f);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->offset_amount == 0.03f);
        CHECK(effect.modifier_animation.values == arm_values[0]);
        saved = *p;
        stock.perpendicular_color.red = 0.f; stock.perpendicular_color.green = 0.f; stock.perpendicular_color.blue = 1.f;
        a = p->perpendicular_color; b = p->parallel_color;
        CHECK(shield_look_colors(body, NULL, &a, &b));
        CHECK(a.red == 0.f && a.green == 0.f && a.blue == 1.f);
        CHECK(!memcmp(p, &saved, sizeof(saved)));
        arm_setting = 0; CHECK(!draw_arms(0, 0, 7, &effect)); CHECK(!effect.modifier_shader);
        setting = "enhanced"; CHECK(shield_look_body(0, &stock.shader) == previous);
        CHECK(!memcmp(previous, &enhanced, sizeof(enhanced)));
        setting = "classic"; CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        setting = "classic_with_sprinkles"; CHECK(shield_look_body(0, &stock.shader) == body);
        shield_look_reset(); CHECK(shield_look_body(0, &stock.shader) != &stock.shader);
        return 0;
    }
    if (!strcmp(argv[1], "colors")) {
        struct shield_look_plasma saved;
        real_rgb_color a, b, original_a, original_b;
        body = shield_look_body(0, &stock.shader);
        saved = *(struct shield_look_plasma *)body;
        original_a = saved.perpendicular_color; original_b = saved.parallel_color;
        a = original_a; b = original_b;
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        CHECK(!memcmp(&a, &original_a, sizeof(a)));
        stock.perpendicular_color.red = 0.243137f; stock.perpendicular_color.blue = 1.f;
        CHECK(shield_look_colors(body, NULL, &a, &b));
        CHECK(a.blue == original_a.red && b.blue == original_b.red);
        CHECK(a.red < a.green && a.green < a.blue);
        CHECK(!memcmp(body, &saved, sizeof(saved)));
        CHECK(!shield_look_colors(&stock.shader, NULL, &a, &b));
        stock.color_source = 3;
        stock.perpendicular_color.red = stock.perpendicular_color.green = stock.perpendicular_color.blue = 1.f;
        memset(&stock.parallel_color, 0, sizeof(stock.parallel_color));
        unit.object.outgoing_change_colors[2].blue = 0.4f;
        a = original_a; b = original_b;
        CHECK(shield_look_colors(body, unit.object.outgoing_change_colors, &a, &b));
        CHECK(a.red == 0.f && a.green == 0.f && a.blue == original_a.red);
        CHECK(b.blue == original_b.red);
        CHECK(draw_arms(0, 0, 7, &effect));
        unit.object.outgoing_change_colors[2].blue = 0.f;
        unit.object.outgoing_change_colors[2].green = 1.f;
        a = original_a; b = original_b;
        CHECK(shield_look_colors(effect.modifier_shader, unit.object.outgoing_change_colors, &a, &b));
        CHECK(a.green == original_a.red && a.blue == 0.f);
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        stock.color_source = 5;
        CHECK(!shield_look_colors(body, unit.object.outgoing_change_colors, &a, &b));
        stock.color_source = 0;
        memset(&stock.perpendicular_color, 0, sizeof(stock.perpendicular_color));
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        stock.perpendicular_color.blue = 1.e-39f;
        a = original_a; b = original_b;
        CHECK(shield_look_colors(body, NULL, &a, &b));
        CHECK(a.blue == original_a.red && a.red == 0.f);
        stock.perpendicular_color.blue = 1.f;
        stock.shader.base.type = 6;
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        stock.shader.base.type = 10;
        CHECK(shield_look_colors(body, NULL, &a, &b));
        CHECK(!memcmp(body, &saved, sizeof(saved)));
        setting = "classic";
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        setting = "enhanced"; shield_look_reset();
        CHECK(!shield_look_colors(body, NULL, &a, &b));
        return 0;
    }
    if (!strcmp(argv[1], "classic")) setting = "classic";
    else if (!strcmp(argv[1], "invalid-setting")) setting = "not-a-profile";
    else if (!strcmp(argv[1], "authored-shader")) { stock.primary_scale = 80.f; stock.offset_amount = 0.f; }
    else if (!strcmp(argv[1], "authored-noise")) noise_matches = 0;
    else if (!strcmp(argv[1], "authored-environment")) stock.shader.base.type = 1;
    else if (!strcmp(argv[1], "authored-nonplasma")) stock.shader.base.type = 6;
    else if (!strcmp(argv[1], "authored-channel")) stock.intensity_source = 3;
    else if (!strcmp(argv[1], "authored-function")) functions[1].period = 2.f;
    else if (!strcmp(argv[1], "authored-hands")) groups[8] = 'spla';
    else if (!strcmp(argv[1], "authored-body")) groups[5] = 'spla';
    else if (!strcmp(argv[1], "authored-decal")) opaque.flags = 1 << 3;
    else if (!strcmp(argv[1], "no-noise")) no_noise = 1;
    else if (!strcmp(argv[1], "missing")) definition.object.modifier_shader.index = NONE;
    if (!strcmp(argv[1], "classic") || !strcmp(argv[1], "invalid-setting") ||
        !strcmp(argv[1], "missing") || !strcmp(argv[1], "no-noise")) {
        CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(!draw_arms(0, 0, 7, &effect)); CHECK(effect.modifier_shader == &stock.shader);
        CHECK(!shield_look_skip_shell(&hands_model, &stock.shader)); return 0;
    }
    if (!strcmp(argv[1], "malformed-noise")) {
        CHECK(!shield_look_read_noise(NONE)); CHECK(!shield_look_read_noise(3));
        bitmap.width = 33; CHECK(!shield_look_read_noise(3)); bitmap.width = 32;
        bitmap_group.bitmaps.count = 2; CHECK(!shield_look_read_noise(3)); return 0;
    }
    body = shield_look_body(0, &stock.shader); CHECK(body != &stock.shader);
    CHECK(draw_arms(0, 0, 7, &effect)); CHECK(effect.modifier_shader != body);
    CHECK(stock.secondary_scale == 6.f); /* source tag was not mutated */
    if (!strncmp(argv[1], "authored-", 9)) {
        CHECK(((struct shield_look_plasma *)body)->primary_scale == 18.f);
        CHECK(((struct shield_look_plasma *)body)->intensity_source == stock.intensity_source);
        CHECK(overshield_effect_intensity_source(0) == stock.intensity_source - 1);
        CHECK(((struct shield_look_plasma *)body)->offset_source == stock.intensity_source);
        CHECK(((struct shield_look_plasma *)body)->primary_map.index == (noise_matches ? 3 : 9));
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->primary_scale == 45.f);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->offset_amount == 0.03f);
        CHECK(shield_look_skip_shell(&hands_model, &stock.shader) == (stock.shader.base.type > 4));
        CHECK(shield_look_skip_shell(&model, &stock.shader) == (stock.shader.base.type > 4));
        CHECK(!shield_look_skip_shell(&model, &opaque.shader));
        setting = "classic";
        CHECK(overshield_effect_intensity_source(0) ==
            (stock.shader.base.type == 10 ? stock.intensity_source - 1 : NONE));
        CHECK(!shield_look_skip_shell(&hands_model, &stock.shader));
        CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(!draw_arms(0, 0, 7, &effect));
        return 0;
    }
    CHECK(effect.modifier_animation.values[1] == 0.5f);
    if (!strcmp(argv[1], "export")) {
        export_shader((struct shield_look_plasma *)body);
        export_shader((struct shield_look_plasma *)effect.modifier_shader); return 0;
    }
    if (!strcmp(argv[1], "arms-off")) {
        arm_setting = 0;
        CHECK(!draw_arms(0, 0, 7, &effect)); CHECK(!effect.modifier_shader);
        CHECK(shield_look_skip_shell(&hands_model, &stock.shader));
        CHECK(shield_look_body(0, &stock.shader) == body);
        arm_setting = 1; CHECK(draw_arms(0, 0, 7, &effect));
        CHECK(effect.modifier_animation.values == arm_values[0]);
        CHECK(effect.modifier_animation.colors == unit.object.outgoing_change_colors);
    } else if (!strcmp(argv[1], "profiles")) {
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->offset_source == stock.offset_source);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->offset_amount == stock.offset_amount);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->offset_exponent == stock.offset_exponent);
        CHECK(effect.modifier_animation.values == arm_values[0]);
        CHECK(((struct shield_look_plasma *)body)->primary_scale == 18.f);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->primary_scale == 45.f);
        CHECK(((struct shield_look_plasma *)effect.modifier_shader)->secondary_scale == 62.f);
        CHECK(((struct shield_look_plasma *)body)->primary_map.index == 3);
        overshield = 1; CHECK(draw_arms(0, 0, 7, &effect)); CHECK(effect.modifier_animation.values[1] == 0.75f);
        CHECK(unit.object.outgoing_function_values[1] == 0.5f);
    } else if (!strcmp(argv[1], "outer")) {
        struct shield_look_plasma original_body, outer_body, original_arms;
        struct shader *original_pointer, *outer_pointer;
        setting = "enhanced";
        original_pointer = shield_look_body(0, &stock.shader);
        original_body = *(struct shield_look_plasma *)original_pointer;
        CHECK(draw_arms(0, 0, 7, &effect));
        original_arms = *(struct shield_look_plasma *)effect.modifier_shader;
        setting = "enhanced2";
        outer_pointer = shield_look_body(0, &stock.shader);
        CHECK(outer_pointer == original_pointer);
        outer_body = *(struct shield_look_plasma *)outer_pointer;
        CHECK(outer_body.offset_source == 2 && outer_body.offset_amount == 0.0125f);
        CHECK(outer_body.offset_exponent == 0.5f);
        outer_body.offset_source = original_body.offset_source;
        outer_body.offset_amount = original_body.offset_amount;
        outer_body.offset_exponent = original_body.offset_exponent;
        CHECK(!memcmp(&outer_body, &original_body, sizeof(outer_body)));
        CHECK(draw_arms(0, 0, 7, &second));
        CHECK(!memcmp(&original_arms, second.modifier_shader, sizeof(original_arms)));
        CHECK(!memcmp(&original_body, original_pointer, sizeof(original_body)));
        setting = "classic"; CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(!draw_arms(0, 0, 7, &second));
        setting = "enhanced2"; CHECK(shield_look_body(0, &stock.shader) == outer_pointer);
        setting = "enhanced"; CHECK(shield_look_body(0, &stock.shader) == original_pointer);
    } else if (!strcmp(argv[1], "split")) {
        unit.object.outgoing_function_values[1] = 0.9f;
        CHECK(draw_arms(1, 0, 7, &second)); CHECK(second.modifier_animation.values != effect.modifier_animation.values);
        CHECK(effect.modifier_animation.values[1] == 0.5f && second.modifier_animation.values[1] == 0.9f);
        CHECK(!draw_arms(-1, 0, 7, &second)); CHECK(!draw_arms(4, 0, 7, &second));
    } else if (!strcmp(argv[1], "toggle")) {
        setting = "classic"; CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(!draw_arms(0, 0, 7, &second));
        setting = argc == 3 ? argv[2] : "enhanced"; CHECK(shield_look_body(0, &stock.shader) == body);
    } else if (!strcmp(argv[1], "reload")) {
        shield_look_reset(); definition.object.modifier_shader.index = NONE;
        CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        shield_look_reset(); definition.object.modifier_shader.index = 2;
        CHECK(shield_look_body(0, &stock.shader) != &stock.shader);
    } else if (!strcmp(argv[1], "capacity")) {
        for (i = shield_look_count; i < SHIELD_LOOK_MAXIMUM; i++) {
            shield_looks[i].definition_index = NONE;
        }
        shield_look_count = SHIELD_LOOK_MAXIMUM; object.definition_index = 6; tags[6] = &definition;
        CHECK(shield_look_body(0, &stock.shader) == &stock.shader);
        CHECK(((struct shield_look_plasma *)body)->primary_scale == 18.f);
    } else { fprintf(stderr, "unknown case: %s\n", argv[1]); return 1; }
    return 0;
}
