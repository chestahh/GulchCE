/* Host-owned bot admission and input replication. The session uses ordinary
player records and actions; unmodified OpenCE clients need no bot logic.
The opt-in machine reservation is fixed when the host is created. */
#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "game/player_queues_new.h"
#include "networking/network_game_globals.h"
#include "bots.h"
#include "main/console.h"
#include <stdio.h>

int config_boolean(const char *name);

static int network_bots;
static byte owned[BOTS_MAXIMUM];

int bots_console_command(char const *expression)
{
	static char const *const commands[] = { "add_bot", "add_bot_red", "add_bot_blue",
		"drop_bot", "drop_bot_red", "drop_bot_blue" };
	char command[32], extra, status[180];
	long index;
	int result;
	if (sscanf(expression, " %31s %c", command, &extra) != 1) return -1;
	for (index = 0; index < (long)NUMBEROF(commands); index++)
		if (!strcmp(command, commands[index]))
		{
			result = bots_manage(index < 3, index % 3 - 1, status, sizeof(status));
			console_printf(TRUE, "%s", status);
			return result;
		}
	return -1;
}

void bots_network_session_begin(void)
{
	bots_management_reset();
	network_bots = config_boolean("bots.network");
	csmemset(owned, 0, sizeof(owned));
}

void bots_network_session_end(void)
{
	bots_management_reset();
	network_bots = FALSE;
	csmemset(owned, 0, sizeof(owned));
}

void bots_player_ownership(long machine_index, int present)
{
	if (bots_machine_is_bot(machine_index))
		owned[machine_index - BOTS_FIRST_MACHINE] = present != 0;
}

int bots_player_owned(long machine_index)
{
	return bots_machine_is_bot(machine_index) && owned[machine_index - BOTS_FIRST_MACHINE];
}

int bots_network_reserved(void)
{
	return network_bots;
}

int bots_session_allowed(void)
{
	return global_network_game_server_get() && (network_game_is_splitscreen_local() || network_bots);
}

void bots_network_record_actions(struct player_action *actions)
{
	struct data_iterator iterator;
	struct player_datum *player;
	if (!game_engine_running() || !bots_session_allowed() || network_game_is_splitscreen_local())
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE && bots_player_owned(player->network_player_data.machine_index))
		{
			long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
			if (index >= 0 && index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
				update_server_note_bot_action((short)index, &actions[index]);
		}
	}
}
