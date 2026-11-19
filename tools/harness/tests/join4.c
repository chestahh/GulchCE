#include "harness.h"

enum { MAXIMUM_LOCAL_PLAYERS = 4, _join4_choosing = 1, _join4_selected };
static struct { int state[4], keyboard_held; } join4;
static int playing[4], added[4], resets, space, keyboard_alone, pads;

static boolean lobby_controller_playing(short port) { return playing[port]; }
static void lobby_join_reset(void) { resets++; }
static int halo_keyboard_join_held(void) { return space; }
static int halo_keyboard_shares_gamepad(void) { return !keyboard_alone; }
static void halo_keyboard_alone_set(boolean enabled) { keyboard_alone = enabled; }
static int halo_gamepad_count(void) { return pads; }
static long player_ui_get_active_player_profile_index(short port) { return port; }
static boolean ui_widget_port_multiplayer_player(short port, long profile)
{
    CHECK(profile == port, "the selected profile belongs to the joining controller");
    added[port]++;
    return TRUE;
}
#define platform_log(...) ((void)0)
#define join4_notice(...) ((void)0)
#include "handlers.inc"

int main(void)
{
    short opener, port;
    for (opener = 0; opener < MAXIMUM_LOCAL_PLAYERS; opener++)
    {
        memset(playing, 0, sizeof(playing));
        memset(added, 0, sizeof(added));
        memset(&join4, 0, sizeof(join4));
        playing[opener] = TRUE;
        resets = 0;
        for (port = 0; port < MAXIMUM_LOCAL_PLAYERS; port++)
        {
            CHECK(join4_back(port) == (port == opener), "only the playing opener can back out");
            if (port != opener) join4.state[port] = _join4_selected;
        }
        CHECK(!join4_back(-1) && !join4_back(4) && resets == 1, "invalid ports cannot cancel");
        join4.state[(opener+1)%4] = _join4_choosing;
        CHECK(!join4_done(opener), "wait for the player choosing a profile");
        join4.state[(opener+1)%4] = _join4_selected;
        space = TRUE;
        CHECK(!join4_done(0), "keyboard SPACE must not finish the lobby");
        space = FALSE;
        CHECK(join4_done(opener) && resets == 2, "the opener can finish profile selection");
        for (port = 0; port < MAXIMUM_LOCAL_PLAYERS; port++)
            CHECK(added[port] == (port != opener), "each newly selected port joins once");
    }
    space = TRUE;
    pads = MAXIMUM_LOCAL_PLAYERS;
    join4_keyboard();
    CHECK(!keyboard_alone, "four occupied ports cannot split the keyboard");
    space = FALSE; join4_keyboard();
    pads--; space = TRUE; join4_keyboard();
    CHECK(keyboard_alone, "an available port can split from the keyboard");
    return 0;
}
