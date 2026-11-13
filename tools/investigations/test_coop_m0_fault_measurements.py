import pytest

from coop_m0_fault_measurements import cross_checks, gate_observations, gate_summary
from coop_m0_identity_measurements import state_values


def row(kind, tick, values, time=10):
    return {'time': time, 'kind': kind, 'v': [tick, *values]}


def test_ready_gate_does_not_hide_failed_object_or_pending_inventory():
    rows = [row('fault_state', 100, [2, 1, 0, 1, 0]),
            row('fault_inventory', 100, [10, -1, 200, 99, 330])]
    observations, trailing = gate_observations(rows)
    result = gate_summary(observations, 9, 20, 2)
    assert trailing is None
    assert result['ready_with_incomplete_state'] == 1
    assert result['ready_missing_target'] == result['ready_wrong_inventory'] == 1
    assert result['ready_while_objects_unsynchronized'] == 1
    assert result['maximum_pending_inventories'] == 1
    assert result['first_complete_after_revert_seconds'] is None


def test_delayed_dependency_repair_keeps_prior_failure_visible():
    rows = []
    for tick, time, exists, held, pending in ((100, 10, 0, -1, 1), (280, 16, 1, 200, 0)):
        rows += [row('fault_state', tick, [3, 1, 1, pending, exists], time),
                 row('fault_inventory', tick, [10, held, 200, -1, 330], time)]
    values, trailing = gate_observations(rows)
    result = gate_summary(values, 9, 20, 3)
    assert result['ready_with_incomplete_state'] == 1
    assert result['first_complete_after_revert_seconds'] == 7
    assert result['incorrect_samples_after_first_complete'] == 0
    assert gate_summary(values, 9, 20, 2)['samples'] == 0


def test_ready_is_not_required_to_measure_repair():
    values, _ = gate_observations([row('fault_state', 100, [2, 0, 0, 0, 1]),
                                   row('fault_inventory', 100, [10, 200, 200, -1, 330])])
    result = gate_summary(values, 9, 20, 2)
    assert result['first_complete_after_revert_seconds'] == 1
    assert result['ready_samples'] == result['ready_with_incomplete_state'] == 0


def test_only_the_final_half_pair_may_be_returned_as_truncated():
    first = row('fault_state', 100, [2, 1, 0, 1, 0])
    assert gate_observations([first]) == ([], first)
    with pytest.raises(ValueError, match='Unpaired'):
        gate_observations([first, first])
    with pytest.raises(ValueError, match='mismatched'):
        gate_observations([first, row('fault_inventory', 101, [10, -1, 200, -1, 330])])


def test_cross_type_requires_client_replacement_not_just_deletion():
    old, new = -131065, -65529
    a = {'rows': {'object': [[old, 100, 2, -1, 10]],
        'inventory': [[10, 0, old], [11, 0, -1]], 'magazine': [[old, 0, 20, 20, 0]]}}
    b = {'rows': {'object': [[new, 200, 0, -1, -1]],
        'inventory': [[10, 0, -1], [11, 0, -1]]}}
    saved = state_values(a, [10, 11], old, [new])
    changed = state_values(b, [10, 11], old, [new])
    assert all(cross_checks(saved, changed, [10, 11], old, new, [100, 200]).values())
    b['rows']['object'] = []
    absent = state_values(b, [10, 11], old, [new])
    assert not cross_checks(saved, absent, [10, 11], old, new, [100, 200])['replacement_biped']
