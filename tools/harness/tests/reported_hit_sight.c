#include "harness.h"
#include <math.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#include "config.inc"

enum { _collision_test_structure_bit, _collision_test_front_facing_surfaces_bit,
    _collision_test_ignore_invisible_surfaces_bit, _collision_test_ignore_breakable_surfaces_bit,
    _collision_test_ignore_two_sided_surfaces_bit };
struct collision_result { int unused; };
static struct damage_trail_point damage_trails[1][PLAYER_TRAIL_POINTS];
static struct { struct damage_stuck_projectile stuck[MAXIMUM_STUCK_PROJECTILES]; } damage_players[1];
static long now = 300;
static int mode, calls;
static boolean blocked_eye_muzzle;
static long game_time_get(void) { return now; }
static real distributed_machine_round_trip_ticks(long machine) { CHECK(machine == 0, "machine"); return 0; }
/* Object discovery is a separate boundary; this fixture supplies host-owned notes. */
static void distributed_note_stuck_projectiles(void) {}

static boolean collision_test_line(unsigned long flags, real_point3d const *from,
    real_point3d const *to, long ignored, struct collision_result *result)
{
    CHECK(flags & FLAG(_collision_test_structure_bit), "structure must be checked");
    CHECK(ignored == NONE, "no object owns the structure trace");
    calls++;
    if (mode == 0) return FALSE;
    if (mode == 1) return TRUE;
    /* A centre ray is blocked; both legs through the impact must be clear. */
    if (mode == 2) return from->x == 0 && to->x == 10;
    if (mode == 3) return to->x == 10;
    /* A mounted muzzle is useful only with a clear eye-to-muzzle segment. */
    if (from->x == 2) return FALSE;
    if (to->x == 2) return blocked_eye_muzzle;
    return TRUE;
}
#include "under_test.inc"

static struct damage_trail_point *point(long time)
{
    struct damage_trail_point *p = &damage_trails[0][(time / PLAYER_TRAIL_INTERVAL_TICKS) & (PLAYER_TRAIL_POINTS - 1)];
    memset(p, 0, sizeof(*p)); p->time = time; p->eye = 1;
    return p;
}
static boolean sight(void)
{
    real_point3d impact = { 9, 0, 1 }, centre = { 10, 0, 1 };
    struct damage_reach reach;
    memset(&reach, 0, sizeof(reach));
    return distributed_report_line_of_sight(0, 0, &impact, &centre, 1, &reach, now);
}
int main(int argc, char **argv)
{
    int i;
    struct damage_trail_point *p;
    struct damage_stuck_projectile *stuck;
    CHECK(argc == 2, "case required");
    for (i = 0; i < PLAYER_TRAIL_POINTS; i++) damage_trails[0][i].time = NONE;
    for (i = 0; i < MAXIMUM_STUCK_PROJECTILES; i++) damage_players[0].stuck[i].time = NONE;
    p = point(now);
    if (!strcmp(argv[1], "clear")) {
        CHECK(sight(), "open line rejected");
    } else if (!strcmp(argv[1], "blocked")) {
        mode = 1; CHECK(!sight(), "solid wall accepted");
    } else if (!strcmp(argv[1], "impact")) {
        mode = 2; CHECK(sight(), "clear two-leg route rejected");
        mode = 3; CHECK(!sight(), "impact-to-centre wall accepted");
    } else if (!strcmp(argv[1], "mounted")) {
        mode = 4; p->mounted = TRUE; p->muzzle.x = 2;
        CHECK(sight(), "mounted weapon rejected");
        blocked_eye_muzzle = TRUE;
        CHECK(!sight(), "muzzle beyond cover accepted");
    } else if (!strcmp(argv[1], "missing")) {
        p->time = NONE; CHECK(!sight() && !calls, "absent living history accepted");
    } else if (!strcmp(argv[1], "window")) {
        p->time = NONE; point(0);
        CHECK(!sight() && !calls, "expired history accepted");
        point(now - PLAYER_TRAIL_INTERVAL_TICKS);
        CHECK(sight(), "recent history rejected");
    } else if (!strcmp(argv[1], "budget")) {
        mode = 1;
        for (i = 0; i < PLAYER_TRAIL_POINTS && now - i * PLAYER_TRAIL_INTERVAL_TICKS >= 0; i++)
            point(now - i * PLAYER_TRAIL_INTERVAL_TICKS);
        CHECK(!sight(), "blocked history accepted");
        CHECK(calls == 2 * REPORT_LINE_OF_SIGHT_POINTS, "trace budget exceeded or truncated: %d", calls);
    } else if (!strcmp(argv[1], "attached")) {
        stuck = &damage_players[0].stuck[0];
        stuck->time = now; stuck->object_index = 7; stuck->damage_index = 9;
        CHECK(!distributed_stuck_detonation(0, 8, 9), "wrong target accepted");
        CHECK(!distributed_stuck_detonation(0, 7, 8), "wrong damage accepted");
        CHECK(distributed_stuck_detonation(0, 7, 9), "host attachment rejected");
        CHECK(!distributed_stuck_detonation(0, 7, 9), "attachment spent twice");
        stuck->spent = FALSE; stuck->time = now - STUCK_PROJECTILE_TICKS - 1;
        CHECK(!distributed_stuck_detonation(0, 7, 9), "expired attachment accepted");
        stuck->time++;
        CHECK(distributed_stuck_detonation(0, 7, 9), "boundary attachment rejected");
    } else CHECK(FALSE, "unknown case");
    return 0;
}