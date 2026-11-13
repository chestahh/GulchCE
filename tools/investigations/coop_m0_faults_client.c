/* M0 generated-only, bounded injections at real reconstruction boundaries. */
extern long m0_fault_epoch(void);
extern int m0_target_read(long *target, long *unit, long *definition);
static long m0_target = NONE, m0_target_unit = NONE, m0_target_definition = NONE;
static boolean m0_allocation_refused, m0_dependency_released;
static long m0_dependency_until = NONE;
static struct distributed_object_change m0_delayed_change;

static boolean m0_fault_target(void)
{
	long target, unit, definition;

	if (!network_game_distributed_client()) return FALSE;
	if (m0_target != NONE) return TRUE;
	if (m0_target_read(&target, &unit, &definition) && target != NONE && unit != NONE)
	{
		m0_target = target;
		m0_target_unit = unit;
		m0_target_definition = definition;
		m0_emit("fault_target", game_time_get(), target, unit, definition, 0, 0);
	}
	return m0_target != NONE;
}

static boolean m0_fault_enabled(void)
{
	char const *mode = getenv("M0_RECOVERY_DOMAINS");
	return mode && !strcmp(mode, "faults");
}

int m0_fault_allocation(long index)
{
	if (m0_fault_enabled() && m0_fault_target() && m0_fault_epoch() == 2 &&
		index == m0_target && !m0_allocation_refused)
	{
		m0_allocation_refused = TRUE;
		m0_emit("allocation_refused", game_time_get(), index, m0_fault_epoch(), 0, 0, 0);
		return TRUE;
	}
	return FALSE;
}

static boolean m0_dependency_hold(struct distributed_object_change const *change)
{
	if (!m0_fault_enabled() || !m0_fault_target() || m0_fault_epoch() != 3 ||
		change->object_index != m0_target || change->definition_index != m0_target_definition || m0_dependency_released)
		return FALSE;
	if (m0_dependency_until == NONE)
	{
		m0_delayed_change = *change;
		m0_dependency_until = game_time_get() + 180;
		m0_emit("dependency_held", game_time_get(), m0_target, m0_fault_epoch(), m0_dependency_until, 0, 0);
	}
	return TRUE;
}

static void m0_dependency_update(void)
{
	if (m0_dependency_until == NONE || m0_dependency_released) return;
	if (m0_fault_epoch() != 3)
	{
		m0_dependency_released = TRUE;
		m0_emit("dependency_cancelled", game_time_get(), m0_fault_epoch(), 0, 0, 0, 0);
		return;
	}
	if (game_time_get() < m0_dependency_until) return;
	m0_dependency_released = TRUE;
	m0_emit("dependency_released", game_time_get(), m0_target, m0_fault_epoch(), 0, 0, 0);
	distributed_client_create(&m0_delayed_change);
}

static void m0_fault_observe(void)
{
	struct unit_datum *unit;
	long weapon = NONE;

	if (!m0_fault_target()) return;
	unit = unit_try_and_get(m0_target_unit);
	if (unit) weapon = unit->unit.weapon_object_indices[0];
	m0_emit("fault_state", game_time_get(), m0_fault_epoch(), distributed_checkpoint_ready(),
		objects_client_synchronized, objects_client_retried_inventory_count, object_try_and_get(m0_target) != NULL);
	m0_emit("fault_inventory", game_time_get(), m0_target_unit, weapon, m0_target,
		objects_client_failed_time, objects_client_ready_interval);
}
