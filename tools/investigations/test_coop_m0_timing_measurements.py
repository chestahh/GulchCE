import pytest

from coop_m0_timing_measurements import ammunition, save_window, timer_error, timer_value


def state(tick, reference=100, remaining=None):
    return {'tick': tick, 'rows': {
        'timer': [[1, 0, 600, reference, 150]],
        'timer_layout': [[20, 30, 0, 600 + reference - tick if remaining is None else remaining, 0]],
        'inventory': [[10, 0, 20, 0, 0], [11, 0, 21, 0, 0]],
        'magazine': [[20, 0, 20, 20, 0], [21, 0, 20, 20, 0]]}}


def test_timer_uses_expiry_tick_not_capture_time():
    assert timer_value(state(200))['remaining'] != timer_value(state(230))['remaining']
    assert timer_error(state(200), 700) == timer_error(state(230), 700) == 0


def test_equal_peers_can_both_lose_saved_countdown():
    # Saved at 190 with 510 ticks remaining; reverted at 370, shifted 180.
    assert timer_error(state(400), 370 + 510) == -180
    assert timer_error(state(430), 370 + 510) == -180
    assert timer_error(state(400, reference=280), 370 + 510) == 0
    expired = state(700)
    assert timer_value(expired)['remaining'] == 0
    assert 370 + 510 - expired['tick'] == 180


def test_formula_disagreement_is_not_silently_accepted():
    assert not timer_value(state(200, remaining=123))['formula_valid']


def test_expired_sentinel_is_not_a_moving_expiry_deadline():
    value = state(700, remaining=-1)
    value['rows']['timer'][0][2] = -1
    assert timer_value(value)['formula_valid']
    assert timer_value(value)['expired']
    assert timer_error(value, 880) is None


def test_warning_rewrite_does_not_hide_early_expiry():
    # Saved 510 remaining; second revert at 640. HUD changes the stored
    # representation to cutoff=150, reference=730, giving expiry=880.
    value = state(660, reference=730, remaining=220)
    value['rows']['timer'][0][2] = 150
    assert timer_value(value)['formula_valid']
    assert timer_error(value, 640 + 510) == -270


def test_counts_require_both_held_membership_and_magazine():
    value = state(200)
    weapons = [[10, 20, 0], [11, 21, 0]]
    expected = [{'held': True, 'counts': [[20, 20]]}] * 2
    assert ammunition(value, weapons) == expected
    value['rows']['inventory'][1][2] = -1
    assert ammunition(value, weapons)[1] == {'held': False, 'counts': [[20, 20]]}
    value['rows']['magazine'].pop()
    assert ammunition(value, weapons)[1]['counts'] == []


def test_save_window_rejects_overwrites_before_either_revert():
    for overwritten_at in (12, 20, 29, 30):
        with pytest.raises(ValueError, match='overwritten checkpoint'):
            save_window([{'time': 10}, {'time': overwritten_at}], 9, 12, 30)


def test_later_save_cannot_change_the_checkpoint_already_read():
    saved, later = save_window([{'time': 1}, {'time': 10}, {'time': 40}], 9, 12, 30)
    assert saved == {'time': 10}
    assert later == [{'time': 40}]
