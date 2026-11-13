/* Diagnostic fixture only, included in a generated network_objects.c copy.
   Uses loaded tags and normal allocation; never edits maps or pool limits. */
static void m0_load_count(char const *kind)
{
	struct object_iterator iterator;
	struct object_datum *object;
	struct objects_information information;
	unsigned long hash = 2166136261UL;
	long count = 0, units = 0, total = 0, high = 0;

	object_iterator_new(&iterator, _object_mask_all, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		total++;
		if (!distributed_object_networked(iterator.index)) continue;
		count++;
		if (TEST_FLAG(_object_mask_unit, object->object.type)) units++;
		high = MAX(high, DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.index) + 1);
		hash = (hash ^ (unsigned long)iterator.index) * 16777619UL;
		hash = (hash ^ (unsigned long)object->definition_index) * 16777619UL;
	}
	m0_emit(kind, game_time_get(), count, units, total, high, hash);
	objects_information_get(&information);
	m0_emit("pool_counts", game_time_get(), information.object_count,
		information.active_object_count, information.used_memory,
		OBJECT_MEMORY_POOL_SIZE, 0);
}

static void m0_load_create(void)
{
	static boolean attempted;
	char const *setting = getenv("M0_LOAD_OBJECTS");
	struct object_iterator iterator;
	struct object_datum *object;
	struct object_placement_data placement;
	real_point3d origin;
	long definition = NONE, requested, created = 0, failed = 0, index;
	unsigned long mask = _object_mask_weapon;
	double began;

	if (attempted || !setting || !network_coop_active()) return;
	attempted = TRUE;
	requested = atol(setting);
	if (requested < 0 || requested > MAXIMUM_TRACKED_OBJECTS)
	{
		m0_emit("load_invalid", requested, 0, 0, 0, 0, 0);
		return;
	}
	if (getenv("M0_LOAD_BIPEDS")) mask = _object_mask_biped;
	object_iterator_new(&iterator, mask, 0);
	object = object_iterator_next(&iterator);
	if (object)
	{
		definition = object->definition_index;
		origin = object->object.position;
	}
	if (definition == NONE)
	{
		m0_emit("load_no_template", requested, mask, 0, 0, 0, 0);
		return;
	}
	m0_load_count("load_before");
	began = coop_m0_clock();
	for (index = 0; index < requested; index++)
	{
		object_placement_data_new(&placement, definition, NONE);
		placement.flags |= FLAG(_new_object_never_automatically_delete_bit);
		placement.position = origin;
		placement.position.x += 4.0f + (real)(index % 64) * 0.75f;
		placement.position.y += (real)(index / 64) * 0.75f;
		if (object_new(&placement) == NONE)
		{
			failed++;
			break;
		}
		created++;
	}
	m0_emit("load_created", requested, created, failed, definition, mask,
		(coop_m0_clock() - began) * 1000.0);
	m0_load_count("load_after");
}
