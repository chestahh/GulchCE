#include "harness.h"
#define HALO_ANDROID 1
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define SET_FLAG(value, bit, enabled) ((value) = ((value) & ~(1UL << (bit))) | ((enabled) ? 1UL << (bit) : 0))
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static void pthread_mutex_lock(pthread_mutex_t *mutex) { (void)mutex; }
static void pthread_mutex_unlock(pthread_mutex_t *mutex) { (void)mutex; }
typedef void JNIEnv;
typedef void *jclass;
typedef int jint;
typedef int jboolean;
#define JNIEXPORT
#define JNICALL
#define JNI_TRUE 1
#define JNI_FALSE 0
static struct { boolean deathless_player, infinite_ammo, bottomless_clip, super_jump; } cheat;
static int enabled, playing, cinematic, network, local, weapons, powerups, camouflage;
static int config_boolean(char const *name) { (void)name; return enabled; }
static int touch_game_playing(void) { return playing; }
static int touch_game_cinematic_playing(void) { return cinematic; }
static boolean network_game_is_active(void) { return network; }
static boolean network_game_is_splitscreen_local(void) { return local; }
static void cheat_all_weapons(void) { weapons++; }
static void cheat_all_powerups(void) { powerups++; }
static void cheat_active_camouflage_local_player(short player) { CHECK(player == 0, "wrong player"); camouflage++; }
#include "under_test.inc"
#define request(value) Java_com_halo_decomp_AndroidCheatMenu_nativeRequest(NULL, NULL, value)
#define state_read() Java_com_halo_decomp_AndroidCheatMenu_nativeState(NULL, NULL)

int main(int argc, char **argv)
{
	int i;
	CHECK(argc == 2, "case");
	playing = 1;
	if (!strcmp(argv[1], "disabled"))
	{
		android_cheats_update();
		CHECK(state_read() == 0 && !request(0x100), "default enabled");
		return 0;
	}
	enabled = 1;
	android_cheats_update();
	CHECK((state_read() & 3) == 3, "offline unavailable");
	if (!strcmp(argv[1], "offline"))
	{
		CHECK(request(0x100) && request(4) && request(5) && request(6), "rejected typed request");
		android_cheats_update();
		CHECK(cheat.deathless_player && weapons == 1 && powerups == 1 && camouflage == 1, "actions not applied");
		CHECK(request(0), "disable request");
		android_cheats_update();
		CHECK(!cheat.deathless_player, "switch not disabled");
	}
	else if (!strcmp(argv[1], "network"))
	{
		CHECK(request(4), "queue before transition");
		network = 1;
		android_cheats_update();
		CHECK(state_read() == 1 && !request(4) && weapons == 0, "network host/client accepted cheat");
		local = 1;
		android_cheats_update();
		CHECK(request(4), "offline split screen rejected");
		android_cheats_update();
		CHECK(weapons == 1, "stale request survived transition");
	}
	else if (!strcmp(argv[1], "reset"))
	{
		cheat.infinite_ammo = TRUE;
		CHECK(request(0x100) && request(1), "switch requests");
		android_cheats_update();
		CHECK(cheat.deathless_player && !cheat.infinite_ammo, "switches");
		CHECK(request(4), "queue at unload");
		android_cheats_reset();
		CHECK(!cheat.deathless_player && cheat.infinite_ammo && host_android_cheats_read() == -1, "state leaked at unload");
		android_cheats_update();
		CHECK(request(0x103), "super jump");
		android_cheats_update();
		cheat.super_jump = FALSE; /* A subsequent console edit owns this value. */
		enabled = 0;
		android_cheats_update();
		CHECK(!cheat.super_jump && state_read() == 0, "disable overwrote subsequent change");
		android_cheats_reset();
		CHECK(cheat.infinite_ammo, "repeated reset lost original setting");
	}
	else if (!strcmp(argv[1], "cinematic"))
	{
		CHECK(request(0x100), "deathless request");
		android_cheats_update();
		CHECK(request(4), "queued before cinematic");
		cinematic = 1;
		android_cheats_update();
		CHECK(cheat.deathless_player && weapons == 0 && !request(4), "cinematic changed cheats or accepted action");
		cinematic = 0;
		android_cheats_update();
		CHECK(weapons == 0 && cheat.deathless_player, "stale cinematic action survived");
		playing = 0;
		android_cheats_update();
		CHECK(!cheat.deathless_player, "cheat survived gameplay exit");
	}
	else if (!strcmp(argv[1], "queue"))
	{
		CHECK(!request(-1) && !request(7) && !request(0x200), "malformed request accepted");
		for (i = 0; i < 8; i++) CHECK(request(4), "bounded queue refused early");
		CHECK(!request(4), "queue overflow");
		android_cheats_update();
		CHECK(weapons == 4, "poll exceeded budget");
		android_cheats_update();
		CHECK(weapons == 8 && host_android_cheats_read() == -1, "queue duplicated/lost request");
	}
	else CHECK(0, "unknown case");
	return 0;
}
