/* One private session per process. Epoch changes discard held descriptions;
 * the production dispatcher independently judges the captured old packets. */
extern long m0_fault_epoch(void);
extern int m0_target_read(long *, long *, long *);
extern int m0_other_read(long *, long *, long *);
extern void m0_pressure_update(void);
extern void m0_stale_replay(void);
static long m0_targets[2] = {NONE, NONE}, m0_units[2], m0_definitions[2];
static struct distributed_object_change m0_held[2];
static boolean m0_have[2], m0_released;
static long m0_hold_epoch, m0_hold_start;

static boolean m0_stress_targets(void)
{
	if (!network_game_distributed_client()) return FALSE;
	if (m0_targets[0] != NONE && m0_targets[1] != NONE) return TRUE;
	if (!m0_target_read(&m0_targets[0], &m0_units[0], &m0_definitions[0]) ||
		!m0_other_read(&m0_targets[1], &m0_units[1], &m0_definitions[1])) return FALSE;
	return m0_targets[0] != NONE && m0_targets[1] != NONE;
}

static boolean m0_stress_enabled(void)
{
	char const *mode = getenv("M0_RECOVERY_DOMAINS");
	return mode && !strcmp(mode, "stress");
}

int m0_stress_pressure_target(long index)
{
	return m0_stress_enabled() && m0_stress_targets() && m0_fault_epoch() == 2 &&
		(index == m0_targets[0] || index == m0_targets[1]);
}

static boolean m0_dependency_hold(struct distributed_object_change const *change)
{
	long epoch = m0_fault_epoch();
	short index;
	if (!m0_stress_enabled() || !m0_stress_targets() || (epoch != 3 && epoch != 4)) return FALSE;
	if (m0_hold_epoch != epoch)
	{
		m0_hold_epoch = epoch;
		m0_hold_start = game_time_get();
		m0_released = FALSE;
		m0_have[0] = m0_have[1] = FALSE;
	}
	if (m0_released) return FALSE;
	for (index = 0; index < 2; index++)
		if (change->object_index == m0_targets[index] && change->definition_index == m0_definitions[index])
		{
			if (!m0_have[index])
			{
				m0_held[index] = *change;
				m0_have[index] = TRUE;
				m0_emit("dependency_hold", game_time_get(), epoch, index, change->object_index, m0_hold_start, 0);
			}
			return TRUE;
		}
	return FALSE;
}

static void m0_dependency_update(void)
{
	short index;
	m0_pressure_update();
	m0_stale_replay();
	if (!m0_hold_epoch || m0_released) return;
	if (m0_fault_epoch() != m0_hold_epoch)
	{
		m0_emit("dependency_cancel", game_time_get(), m0_hold_epoch, m0_fault_epoch(), m0_have[0], m0_have[1], 0);
		m0_released = TRUE;
		m0_have[0] = m0_have[1] = FALSE;
		return;
	}
	if (m0_hold_epoch != 3 || game_time_get() - m0_hold_start < 180) return;
	m0_released = TRUE;
	for (index = 0; index < 2; index++)
		if (m0_have[index])
		{
			m0_emit("dependency_release", game_time_get(), m0_hold_epoch, index, m0_targets[index], 0, 0);
			distributed_client_create(&m0_held[index]);
		}
}

static void m0_fault_observe(void)
{
	struct unit_datum *unit;
	short index;
	if (!m0_stress_targets()) return;
	m0_emit("stress_gate", game_time_get(), m0_fault_epoch(), distributed_checkpoint_ready(),
		objects_client_synchronized, objects_client_retried_inventory_count, objects_client_retry_ticks);
	for (index = 0; index < 2; index++)
	{
		unit = unit_try_and_get(m0_units[index]);
		m0_emit("stress_inventory", game_time_get(), index, m0_targets[index],
			object_try_and_get(m0_targets[index]) != NULL,
			unit ? unit->unit.weapon_object_indices[0] : NONE, objects_client_failed_time);
	}
}
