import pytest

from coop_m0_stress_measurements import gate_observations, summarize_gate


def records(tick, time, queued, second_exists):
    return [{'time': time, 'kind': 'stress_gate', 'v': [tick, 3, 1, 1, queued, 330]},
            {'time': time, 'kind': 'stress_inventory', 'v': [tick, 0, 100, 1, 100, -1]},
            {'time': time, 'kind': 'stress_inventory', 'v': [tick, 1, 200, second_exists, 200 if second_exists else -1, -1]}]


def test_empty_queue_does_not_prove_both_dependencies_restored():
    values, trailing = gate_observations(records(90, 3, 0, 0))
    result = summarize_gate(values, 0)
    assert trailing is None and result['both_flags_ready_incomplete'] == 1
    assert result['first_complete_seconds'] is None


def test_early_failure_and_later_regression_survive_eventual_repair():
    values, _ = gate_observations(records(90, 3, 2, 0) + records(180, 6, 0, 1) + records(210, 7, 1, 0))
    result = summarize_gate(values, 0)
    assert result['first_complete_seconds'] == 6
    assert result['ready_incomplete'] == 2 and result['incorrect_after_first_complete'] == 1


def test_misordered_or_missing_target_records_are_not_accepted():
    rows = records(90, 3, 0, 1)
    with pytest.raises(ValueError, match='Mismatched'):
        gate_observations([rows[0], rows[2], rows[1]])
    with pytest.raises(ValueError, match='Incomplete'):
        gate_observations(rows[:2] + records(91, 4, 0, 1))


def test_only_trailing_partial_gate_is_reported_separately():
    values, trailing = gate_observations(records(90, 3, 0, 1)[:2])
    assert not values and len(trailing['targets']) == 1
