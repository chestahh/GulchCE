/* Android cheat menu inspired by thelinkin3000/halo-ce-universal's overlay
(406f793f4c08751f854a3615ed244ea3b1f90b88), which credits hardran3's
OpenCE-Ultrahand and HaloDrones packages. The fork's general console and
renderer overlay are not imported; these requests use existing cheat APIs.
See docs/reading/bots_and_android_features.md for credits and constraints. */
#include "cseries.h"
#include "android_cheats.h"

#ifdef HALO_ANDROID
#include "game/cheats.h"
#include "networking/network_game_globals.h"

int config_boolean(char const *name);
int touch_game_playing(void);
int touch_game_cinematic_playing(void);
void host_android_cheats_state(int state);
int host_android_cheats_read(void);

/* IDs 0..3 are switches; 4..6 are one-shot actions. The Android UI and
host queue use the same IDs. Bit 8 carries the requested switch value. */
static boolean *const switches[] =
{
	&cheat.deathless_player, &cheat.infinite_ammo, &cheat.bottomless_clip, &cheat.super_jump
};
static boolean previous[4], applied[4];
static unsigned long owned;

void android_cheats_reset(void)
{
	long index;
	for (index = 0; index < (long)NUMBEROF(switches); index++)
	{
		/* Leave a subsequent console/script change alone. */
		if (TEST_FLAG(owned, index) && *switches[index] == applied[index])
			*switches[index] = previous[index];
	}
	owned = 0;
	host_android_cheats_state(0);
}

void android_cheats_update(void)
{
	int enabled = config_boolean("android.cheat_menu");
	int allowed = enabled && touch_game_playing() &&
		(!network_game_is_active() || network_game_is_splitscreen_local());
	int request, count, state = enabled ? 1 : 0;
	long index;
	if (!allowed)
	{
		android_cheats_reset();
		host_android_cheats_state(state);
		return;
	}
	/* Four requests at most per input poll; the host queue is bounded too. */
	for (count = 0; !touch_game_cinematic_playing() && count < 4 && (request = host_android_cheats_read()) >= 0; count++)
	{
		index = request & 0xFF;
		if ((request & ~0x107) != 0 || index > 6) continue;
		if (index < (long)NUMBEROF(switches))
		{
			if (!TEST_FLAG(owned, index)) previous[index] = *switches[index];
			SET_FLAG(owned, index, TRUE);
			applied[index] = (request & 0x100) != 0;
			*switches[index] = applied[index];
		}
		else if (index == 4) cheat_all_weapons();
		else if (index == 5) cheat_all_powerups();
		else if (index == 6) cheat_active_camouflage_local_player(0);
	}
	if (!touch_game_cinematic_playing()) state |= 2;
	for (index = 0; index < (long)NUMBEROF(switches); index++)
		if (*switches[index]) state |= 1 << (8 + index);
	host_android_cheats_state(state);
}
#endif
