"""The domain oracle must require intended mutations and explicit absence."""
from copy import deepcopy

from coop_m0_domain_measurements import mutation_checks, state_values
from coop_m0_recovery_measurements import compare_fields


def test_presence_identity_and_inventory_do_not_accept_stale_objects():
    ids = dict(enumerate((10, 11, 12, 13, 14)))
    rows = {'object': [[10, 100, 2, -1, -1], [11, 100, 2, -1, -1], [12, 100, 2, -1, 11], [13, 100, 2, -1, -1]],
            'object_pose': [[13, 0, 0, 0, .5]], 'timer': [[1, 1, 3600, 100, 300]],
            'timer_layout': [[20, 30, 0, 3600, 0]], 'inventory': [[1, 0, -1, 0, 0]],
            'allegiance': [[3, 1, 1, 0, 0]], 'magazine': [[13, 0, 20, 0, 0]]}
    expected = state_values({'rows': rows}, 1, ids)
    assert expected['presence'] == [True, True, True, True, False]
    rows['magazine'].append([13, 1, 999, 999, 6])
    assert state_values({'rows': rows}, 1, ids)['magazines'] == [[0, 20, 0]]
    stale = deepcopy(rows)
    stale['object'].append([14, 100, 2, -1, -1])
    stale['inventory'][0][2] = 14
    observed = state_values({'rows': stale}, 1, ids)
    assert set(compare_fields(expected, observed, expected)) == {'presence', 'identity', 'inventory'}


def test_intended_mutation_is_required_not_arbitrary_difference():
    ids = dict(enumerate((10, 11, 12, 13, 14)))
    a = {'presence': [True] * 4 + [False], 'attachment': 11, 'scale': .5,
         'magazines': [[0, 20, 0, 0]], 'inventory': [[1, 0, -1]],
         'timer': [1, 1, 3600, 300], 'timer_layout': [20, 30, 0, 3600],
         'allegiance': [[3, 1, 1]], 'mcc_state': []}
    b = deepcopy(a)
    b.update(presence=[False] + [True] * 4, attachment=-1, scale=2,
             magazines=[[0, 4, 0, 0]], inventory=[[1, 0, 14]],
             timer=[1, 1, 5400, 600], timer_layout=[100, 80, 0, 5400], allegiance=[[3, 0, 0]])
    assert all(mutation_checks(a, b, True, ids).values())
    b['inventory'] = [[1, 0, 999]]
    b['attachment'] = 999
    assert not mutation_checks(a, b, True, ids)['inventory_changed']
    assert not mutation_checks(a, b, True, ids)['attachment_removed']
    assert not all(mutation_checks(a, a, True, ids).values())


def test_mcc_missing_extra_slots_are_not_reported_as_qualified():
    # Ordinary fields intentionally do not mutate in this minimal metadata check.
    a = {'presence': [True] * 4 + [False], 'attachment': 11, 'scale': .5, 'magazines': [],
         'inventory': [], 'timer': [1, 1, 3600, 300], 'timer_layout': [20, 30, 0, 3600],
         'allegiance': [[3, 1, 1]], 'mcc_state': [[.75, .25, .5, 2, 0]], 'mcc_extra_grenades': []}
    b = deepcopy(a)
    b['mcc_state'] = [[.5, .75, .875, 2, 0]]
    checks = mutation_checks(a, b, False, dict(enumerate((10, 11, 12, 13, 14))))
    assert checks['mcc_speed_changed'] and checks['mcc_gains_changed']
    assert 'mcc_extra_grenades_changed' not in checks
