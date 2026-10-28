#include "harness.h"
#include <math.h>
#include <float.h>
#define REAL_MAX FLT_MAX
#define BOTS_MAXIMUM 15
enum { _equipment_powerup_health, _equipment_powerup_overshield, _equipment_powerup_active_camouflage };
enum { _unit_active_camouflaged_bit };
struct bots_bot { long player_index; real_point3d goal; };
struct player_datum { long team_index; };
struct unit_datum {
	struct { real body_vitality, shield_vitality; } object;
	struct { unsigned long flags; } unit;
};
static struct { struct { int powerup_type; } equipment; } equipment;
#define equipment_definition_get(index) (&equipment)
static struct { struct {
	boolean present[2], at_home[2], home_to_score;
	long carrier_player_index[2];
	real_point3d home[2], position[2];
} ctf; } bots_globals;
static boolean defender;
static void bots_ctf_update(void) {}
static boolean bots_ctf_defender(struct bots_bot *bot, struct player_datum *player)
{ (void)bot; (void)player; return defender; }
static boolean bots_ctf_set_goal(struct bots_bot *bot, real_point3d const *point, real moved)
{ (void)moved; bot->goal = *point; return TRUE; }
#include "under_test.inc"

int main(int argc, char **argv)
{
	struct bots_bot bot = { 42, { 0, 0, 0 } };
	struct player_datum player = { 0 };
	real_point3d position = { 0, 0, 0 };
	struct unit_datum unit = { { 1.0f, 1.0f }, { 0 } };
	CHECK(argc == 2, "case");
	bots_globals.ctf.present[0] = bots_globals.ctf.present[1] = TRUE;
	bots_globals.ctf.at_home[0] = bots_globals.ctf.at_home[1] = TRUE;
	bots_globals.ctf.carrier_player_index[0] = bots_globals.ctf.carrier_player_index[1] = NONE;
	bots_globals.ctf.home[0].x = 10;
	bots_globals.ctf.position[0].x = 20;
	if (!strcmp(argv[1], "carrier"))
	{
		bots_globals.ctf.carrier_player_index[1] = 42;
		CHECK(bots_ctf_urgent_goal(&bot, &player, &position) && bot.goal.x == 10, "carrier did not go home");
		player.team_index = 2;
		CHECK(!bots_ctf_urgent_goal(&bot, &player, &position), "invalid team accepted");
	}
	else if (!strcmp(argv[1], "standoff"))
	{
		bots_globals.ctf.home_to_score = TRUE;
		bots_globals.ctf.carrier_player_index[0] = 1;
		bots_globals.ctf.carrier_player_index[1] = 2;
		position.x = 100;
		CHECK(bots_ctf_urgent_goal(&bot, &player, &position) && bot.goal.x == 20, "standoff did not recover own flag");
	}
	else if (!strcmp(argv[1], "recovery"))
	{
		bots_globals.ctf.at_home[0] = FALSE;
		CHECK(bots_ctf_urgent_goal(&bot, &player, &position) && bot.goal.x == 20, "near dropped flag ignored");
		position.x = 100;
		CHECK(!bots_ctf_urgent_goal(&bot, &player, &position), "distant attacker diverted");
		defender = TRUE;
		CHECK(bots_ctf_urgent_goal(&bot, &player, &position), "defender ignored own flag");
	}
	else if (!strcmp(argv[1], "powerups"))
	{
		equipment.equipment.powerup_type = _equipment_powerup_health;
		CHECK(enhanced_bots_powerup_score(&unit, 0, 10) == REAL_MAX, "healthy bot seeks health");
		unit.object.body_vitality = 0.1f;
		CHECK(enhanced_bots_powerup_score(&unit, 0, 10) < 0, "critical health not prioritized");
		equipment.equipment.powerup_type = _equipment_powerup_overshield;
		unit.object.shield_vitality = 2;
		CHECK(enhanced_bots_powerup_score(&unit, 0, 1) == REAL_MAX, "overshield wasted");
		equipment.equipment.powerup_type = _equipment_powerup_active_camouflage;
		CHECK(enhanced_bots_powerup_score(&unit, 0, 1) < 0, "camouflage ignored");
		unit.unit.flags = FLAG(_unit_active_camouflaged_bit);
		CHECK(enhanced_bots_powerup_score(&unit, 0, 1) == REAL_MAX, "camouflage wasted");
	}
	else CHECK(0, "unknown case");
	return 0;
}
