"""Host line-of-sight admission and attached-hit consumption, from production code."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run, structure

CASES = ['clear', 'blocked', 'impact', 'mounted', 'missing', 'window', 'budget', 'attached']
CONTROLS = [
    ('return !collision_test_line(flags, from, centre, NONE, &collision) ||',
     'return TRUE ||', 'blocked'),
    ('!collision_test_line(flags, impact, centre, NONE, &collision)',
     'TRUE', 'impact'),
    ('trail_point->mounted && !collision_test_line(flags, &eye, &trail_point->muzzle, NONE, &collision)',
     'trail_point->mounted', 'mounted'),
    ('stuck->spent = TRUE;', 'stuck->spent = FALSE;', 'attached'),
]


def generated(fault=None):
    source = read('port/linux/game/network_damage.c')
    config = '#define TICKS_PER_SECOND %d\n' % constant(read('source/cseries/cseries.h'), 'TICKS_PER_SECOND')
    config += enum_with(source, 'REPORT_LINE_OF_SIGHT_TICKS =') + '\n'
    for name in ['damage_reach', 'damage_trail_point', 'damage_stuck_projectile']:
        config += structure(source.replace('static struct ', 'struct '), name).rsplit('}', 1)[0] + '};\n'
    code = '\n'.join(function(source, name) for name in [
        'distributed_distance_squared', 'distributed_report_sight_clear',
        'distributed_report_line_of_sight', 'distributed_stuck_detonation'])
    if fault:
        code = mutated(code, *fault)
    return (('config.inc', config), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('reported_hit_sight', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', CONTROLS)
def test_negative_control(before, after, case):
    status, output = run(build('reported_hit_sight', generated((before, after))), case)
    assert status == CHECK_FAILED, output


def test_host_wiring_and_cleanup():
    source = read('port/linux/game/network_damage.c')
    valid = function(source, 'distributed_report_valid')
    assert valid.index('distributed_report_paid(') < valid.index('distributed_report_line_of_sight(')
    assert '!melee && !collision && !area' in valid
    assert 'distributed_stuck_detonation(' in valid
    assert 'distributed_note_stuck_projectiles();' in function(source, 'network_damage_host_tick')
    reset = function(source, 'network_damage_new_game')
    assert 'stuck[index].time = NONE;' in reset
    assert 'damage_trails[player_index][index].time = NONE;' in reset