from copy import deepcopy

from coop_m0_identity_measurements import mutation_checks, ready_observations, reused_slot, state_values
from coop_m0_recovery_measurements import compare_fields

OLD, NEW = -131065, -65529  # Same slot 7, distinct signed 32-bit generations.


def state(changed=False):
    weapon, tag, unit, player, rounds = (NEW, 200, 11, 101, 4) if changed else (OLD, 100, 10, 100, 20)
    return {'rows': {'object': [[weapon, tag, 2, -1, -1], [10, 901, 0, -1, -1], [11, 901, 0, -1, -1]],
        'inventory': [[10, 0, weapon if unit == 10 else -1], [11, 0, weapon if unit == 11 else -1]],
        'identity_owner': [[weapon, player, 1, 1, 0]],
        'magazine': [[weapon, 0, rounds, rounds, 0]]}}


def values(value):
    return state_values(value, [10, 11], OLD, [NEW])


def test_reuse_requires_new_generation_in_same_slot():
    assert reused_slot(OLD, NEW)
    assert not reused_slot(OLD, OLD)
    assert not reused_slot(OLD, NEW + 1)
    assert not reused_slot(-1, 65535)


def test_same_slot_does_not_mean_saved_identity_restored():
    a, b = values(state()), values(state(True))
    differences = compare_fields(a, b, a)
    assert 'slot_contents' in differences
    assert 'post_save_presence' in differences
    assert all(mutation_checks(a, b, [10, 11], [100, 101], [100, 200], OLD, NEW).values())


def test_correct_handle_with_wrong_tag_still_fails():
    changed = state()
    changed['rows']['object'][0][1] = 200
    assert 'slot_contents' in compare_fields(values(state()), values(changed), values(state()))


def test_stale_inventory_and_wrong_owner_are_separate_failures():
    changed = state()
    changed['rows']['inventory'] = [[10, 0, -1], [11, 0, OLD]]
    differences = compare_fields(values(state()), values(changed), values(state()))
    assert 'inventories' in differences and 'all_tracked_inventory_references' in differences
    assert 'owners' not in differences
    changed = state()
    changed['rows']['identity_owner'][0][1] = 101
    assert 'owners' in compare_fields(values(state()), values(changed), values(state()))


def test_post_save_filler_must_be_absent_even_in_another_slot():
    original = state()
    changed = deepcopy(original)
    changed['rows']['object'].append([NEW + 1, 200, 2, -1, -1])
    a = state_values(original, [10, 11], OLD, [NEW, NEW + 1])
    b = state_values(changed, [10, 11], OLD, [NEW, NEW + 1])
    assert b['post_save_presence'] == [False, True]
    assert list(compare_fields(a, b, a)) == ['post_save_presence']


def test_expected_mutations_cannot_be_replaced_by_arbitrary_difference():
    a, b = values(state()), values(state(True))
    b['primary_magazines'][0][2:] = [3, 3]
    assert not mutation_checks(a, b, [10, 11], [100, 101], [100, 200], OLD, NEW)['replacement_primary_rounds']


def test_ready_observations_include_early_wrong_state_in_new_epoch():
    states = []
    for time, epoch, ready, changed in ((10.1, 1, 1, True), (10.25, 2, 1, True),
                                      (10.5, 2, 1, False), (10.75, 2, 0, True)):
        value = state(changed)
        value['time'] = time
        value['rows']['epoch'] = [[epoch, ready]]
        states.append(value)
    result = ready_observations(states, 10, 12, 2, [10, 11], OLD, [NEW], values(state()))
    assert result['samples'] == 2
    assert result['first_capture_after_revert_seconds'] == .25
    assert result['wrong_saved_value_samples']['slot_contents'] == 1
    assert result['unresolved_references'] == 0
