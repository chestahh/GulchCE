#include "harness.h"
#include <math.h>
#include <float.h>
#define TICKS_PER_SECOND 30
#define REAL_MAX FLT_MAX
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PIN(a,l,h) ((a)<(l)?(l):(a)>(h)?(h):(a))
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
enum { _game_mode_swat = 1, _object_dead_bit = 0, _bots_goal_objective = 1 };
struct bots_bot { long player_index, aim_target, aim_until, guard_until_tick; unsigned long random;
    boolean aim_head, aim_feet; short goal_kind; real_point3d goal; };
struct player_datum { long unit_index; };
struct object_datum { long definition_index; struct { real_point3d position; real shield_vitality;
    unsigned long damage_flags; } object; };
struct data_iterator { long datum_index; };
static struct player_datum players[3] = {{0},{1},{2}};
static struct object_datum units[3];
static int zombies[3], mode, scans, player_data, ground_valid = 1;
static long now = 100;
static char const *weapon_name = "weapons\\pistol\\pistol";
static real_point3d points[2] = {{10,0,0},{100,0,0}};
static struct { struct bots_bot bots[1]; long roam_point_count; real_point3d *roam_points; } bots_globals;
static long game_time_get(void) { return now; }
static int game_modes_mode(void) { return mode; }
static struct player_datum *player_get(long i) { return &players[i]; }
static struct object_datum *object_get(long i) { return &units[i]; }
static struct object_datum *object_try_and_get(long i) { return i >= 0 && i < 3 ? &units[i] : NULL; }
static struct object_datum *weapon_get(long i) { (void)i; return &units[0]; }
static char const *tag_get_name(long i) { (void)i; return weapon_name; }
static void unit_get_head_position(long i, real_point3d *point) { *point = units[i].object.position; point->z += 1.5f; }
static long bots_surface_below(real_point3d const *point, real_point3d *ground)
{ *ground = *point; ground->z = 0; return ground_valid ? 0 : NONE; }
static boolean infection_unit_is_zombie(long i) { return zombies[i]; }
static void data_iterator_new(struct data_iterator *it, int data) { (void)data; it->datum_index = -1; scans++; }
static void *data_iterator_next(struct data_iterator *it) { return ++it->datum_index < 3 ? &players[it->datum_index] : NULL; }
static boolean bots_ctf_set_goal(struct bots_bot *bot, real_point3d const *point, real moved)
{ (void)moved; bot->goal = *point; bot->goal_kind = _bots_goal_objective; return TRUE; }
#include "under_test.inc"

int main(int argc, char **argv)
{
    struct bots_bot *bot = &bots_globals.bots[0];
    real_point3d eye = {0,0,2}, aim = {5,0,1}, position = {0,0,0};
    unsigned long random;
    CHECK(argc == 2, "case");
    bot->aim_target = NONE;
    units[1].object.position.x = 5;
    units[2].object.position.x = 100;
    bots_globals.roam_point_count = 2; bots_globals.roam_points = points;
    if (!strcmp(argv[1], "precision") || !strcmp(argv[1], "swat"))
    {
        if (!strcmp(argv[1], "swat")) { mode = _game_mode_swat; weapon_name = "assault rifle"; }
        else units[1].object.shield_vitality = 1;
        enhanced_bots_aim(0, 3, 1, 0, &eye, &aim);
        CHECK(aim.z == (mode ? 1.5f : 1.0f), "shield/SWAT aim rule");
        random = bot->random;
        units[1].object.shield_vitality = 0;
        enhanced_bots_aim(0, 3, 1, 0, &eye, &aim);
        CHECK(aim.z == 1.5f && random == bot->random, "head choice rerolled during burst");
    }
    else if (!strcmp(argv[1], "explosive"))
    {
        weapon_name = "rocket launcher";
        enhanced_bots_aim(0, 3, 1, 0, &eye, &aim);
        CHECK(aim.z == 0.1f, "grounded rocket target not aimed at feet");
        units[1].object.position.z = 3; aim.z = 1;
        enhanced_bots_aim(0, 3, 1, 0, &eye, &aim);
        CHECK(aim.z == 1, "airborne target aimed at ground");
        units[1].object.position.z = 0; ground_valid = 0;
        enhanced_bots_aim(0, 3, 1, 0, &eye, &aim);
        CHECK(aim.z == 1, "missing ground accepted");
    }
    else if (!strcmp(argv[1], "infection"))
    {
        zombies[0] = 1;
        CHECK(bots_infection_goal(bot, &players[0], &position) && bot->goal.x == 5, "zombie ignored nearest human");
        bots_infection_goal(bot, &players[0], &position);
        CHECK(scans == 1, "role snapshot rescanned within tick");
        zombies[1] = 1; now += 30;
        CHECK(bots_infection_goal(bot, &players[0], &position) && bot->goal.x == 100, "converted target retained");
        zombies[0] = 0; bot->goal_kind = 0; now += 30;
        CHECK(bots_infection_goal(bot, &players[0], &position) && bot->goal.x == 100, "human failed to regroup");
    }
    else if (!strcmp(argv[1], "alone"))
    {
        zombies[1] = zombies[2] = 1; units[2].object.position.x = 6;
        CHECK(bots_infection_goal(bot, &players[0], &position) && bot->goal.x == 100, "last human approached zombies");
    }
    else CHECK(0, "case");
    return 0;
}
