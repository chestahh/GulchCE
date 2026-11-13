"""Guard the recovery oracle against false agreement and incomplete evidence."""
from copy import deepcopy

import pytest

from coop_m0_recovery_measurements import captures, compare_fields, one, relationships, stable_objects


def test_agreeing_peers_can_both_have_wrong_saved_state():
    saved = {'vitality': [1, 1], 'objective': 1}
    host = {'vitality': [2, 3], 'objective': 1}
    client = dict(host)
    assert not compare_fields(host, client, saved.keys())
    assert set(compare_fields(saved, host, saved.keys())) == {'vitality'}
    assert set(compare_fields(saved, client, saved.keys())) == {'vitality'}


def test_references_detect_absent_unit_parent_and_weapon():
    state = {'rows': {'roster': [[1, 10, 0, 0, 0]], 'object': [[10, 20, 0, -1, -1]],
                      'inventory': [[10, 0, -1, 0, 0]]}}
    assert relationships(state) == []
    broken = deepcopy(state)
    broken['rows']['roster'][0][1] = 11
    broken['rows']['object'][0][4] = 12
    broken['rows']['inventory'][0][2] = 13
    assert {failure[0] for failure in relationships(broken)} == {
        'roster_unit', 'object_parent', 'inventory_reference'}


def test_stable_objects_detect_missing_and_wrong_parent_without_using_pose():
    host = {'rows': {'object': [[1, 100, 0, -1, -1], [2, 200, 2, -1, 1], [3, 300, 6, 0, -1]]}}
    assert stable_objects(host, host, host)['mismatch_count'] == 0
    client = deepcopy(host)
    client['rows']['object'].pop(0)
    client['rows']['object'][0][4] = -1
    result = stable_objects(host, host, client)
    assert result['stable_host_objects'] == 2
    assert result['mismatch_count'] == 2


def test_capture_associates_helpers_by_time_not_reused_tick(tmp_path):
    (tmp_path / 'm0-network_coop.csv').write_text(
        '1,state_begin,30,0,1,0,0,0\n2,state_end,30,0,0,0,0,0\n'
        '3,state_begin,30,0,1,0,0,0\n4,state_end,30,0,0,0,0,0\n')
    (tmp_path / 'm0-hud_messaging.csv').write_text(
        '1.5,objective,30,1,0,0,0,0\n3.5,objective,30,2,0,0,0,0\n')
    states, _ = captures(tmp_path)
    assert [one(s, 'objective')[0] for s in states] == [1, 2]
    with pytest.raises(ValueError, match='Expected one epoch'):
        one(states[0], 'epoch')


def test_truncated_capture_is_not_recovery_evidence(tmp_path):
    (tmp_path / 'm0-network_coop.csv').write_text('1,state_begin,30,0,1,0,0,0\n')
    with pytest.raises(ValueError, match='Truncated'):
        captures(tmp_path)
    states, rows = captures(tmp_path, allow_truncated_tail=True)
    assert states == []
    assert rows[-1]['kind'] == 'discarded_capture'
