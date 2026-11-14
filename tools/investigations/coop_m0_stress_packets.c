/* Retain one validated envelope of each kind, then submit unchanged bytes
 * after the next epoch through the ordinary dispatcher. No wire edits. */
static word m0_old_packets[2][32768];
static word m0_old_sizes[2];
static boolean m0_old_replayed;

static void m0_stale_remember(void const *message, word size, struct distributed_message_header const *header)
{
	short slot;
	char const *mode = getenv("M0_RECOVERY_DOMAINS");
	if (!mode || strcmp(mode, "stress") || !network_game_distributed_client() ||
		header->checkpoint_epoch != 4 || !header->count || distributed_handling_batch ||
		network_connection_last_read_was_unreliable()) return;
	slot = header->type == _distributed_message_object_changes ? 0 :
		(header->type == _distributed_message_inventories ? 1 : NONE);
	if (slot == NONE || m0_old_sizes[slot]) return;
	csmemcpy(m0_old_packets[slot], message, size);
	m0_old_sizes[slot] = size;
	m0_emit("old_packet_saved", game_time_get(), slot, header->checkpoint_epoch, header->type, size, header->count);
}

void m0_stale_replay(void)
{
	short slot;
	if (m0_old_replayed || distributed_checkpoint_epoch != 5 || !distributed_checkpoint_ready() ||
		!m0_old_sizes[0] || !m0_old_sizes[1]) return;
	m0_old_replayed = TRUE;
	for (slot = 0; slot < 2; slot++)
	{
		m0_emit("old_packet_replay", game_time_get(), slot, 4, distributed_checkpoint_epoch, m0_old_sizes[slot], 0);
		network_distributed_handle_message(NONE, m0_old_packets[slot], m0_old_sizes[slot]);
	}
}
