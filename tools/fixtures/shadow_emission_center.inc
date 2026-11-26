/* Capture-only hypothesis: replace the grenade light's model origin with
   the stock plasma particle emission centre. No random samples or simulation
   updates. Ambiguous/missing emitters retain the original light position. */
int near_emission_origin(long owner, real_point3d *position)
{
    struct data_iterator iterator;
    struct effect_datum *effect;
    real_point3d result;
    int found = 0;
    long particle_tag = tag_loaded('part', "weapons\\plasma grenade\\plasma grenade armed");
    if (owner == NONE || particle_tag == NONE || !effect_data) return FALSE;
    data_iterator_new(&iterator, effect_data);
    while ((effect = data_iterator_next(&iterator)) != NULL)
    {
        struct effect_definition *definition;
        struct effect_event_definition *event;
        long i;
        if (effect->owner_object_index != owner || effect->object_index != owner ||
            TEST_FLAG(effect->header.flags, _effect_stopped_bit)) continue;
        definition = effect_definition_get(effect->definition_index);
        if (effect->event_index < 0 || effect->event_index >= definition->events.count) continue;
        event = TAG_BLOCK_GET_ELEMENT(&definition->events, effect->event_index, struct effect_event_definition);
        for (i = 0; i < event->particles.count && i < MAXIMUM_EFFECT_PARTICLES_PER_EVENT; i++)
        {
            struct effect_particles_definition *particles = TAG_BLOCK_GET_ELEMENT(
                &event->particles, i, struct effect_particles_definition);
            struct effect_location_datum *instance;
            long location;
            if (particles->particle.index != particle_tag || !effect->particle_counts[i] ||
                particles->location_index < 0 || particles->location_index >= definition->locations.count ||
                particles->location_index >= MAXIMUM_EFFECT_LOCATIONS) continue;
            location = effect->location_datum_indices[particles->location_index];
            while ((instance = effect_location_get_next_instance(effect, &location,
                _effect_camera_mode_third_person_only)) != NULL)
            {
                real_vector3d offset;
                real_point3d point;
                if (++found > 1) return FALSE;
                matrix4x3_transform_vector(&instance->matrix, &particles->offset, &offset);
                point.x = instance->matrix.position.x + offset.i;
                point.y = instance->matrix.position.y + offset.j;
                point.z = instance->matrix.position.z + offset.k;
                if (instance->node_designator != NONE)
                    matrix4x3_transform_point(effect_get_node_matrix(effect, instance->node_designator), &point, &result);
                else result = point;
            }
        }
    }
    if (found) *position = result;
    return found;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
