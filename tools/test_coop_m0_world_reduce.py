"""Protect M0 evidence interpretation against missing and ambiguous samples."""
import io
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).parent / 'investigations'))
from coop_m0_world_reduce import frames, records, summarize


def row(wall, kind, *values):
    return ','.join(map(str, [wall, kind, *values, *([0] * (6-len(values)))])) + '\n'


def frame(wall, tick, handle=-65535, floor_type=2):
    return ''.join([
        row(wall, 'world_begin', tick, 1, 1, 0, 1),
        row(wall, 'world_object', tick, handle, 0, -1, -1, 4),
        row(wall, 'world_pose', tick, handle, 1, 2, 3, 0),
        row(wall, 'world_health', tick, handle, 1, 1, 0),
        row(wall, 'world_control', tick, handle, -100, -1, 0, -1),
        row(wall, 'world_throttle', tick, handle, 0, 0, 0, 0),
        row(wall, 'world_floor', tick, handle, 1, floor_type, 777, 3),
        row(wall, 'world_end', tick),
    ])


def test_repeated_ticks_remain_distinct_and_full_handles_are_preserved():
    result = summarize(io.StringIO(frame(1, 30) + frame(2, 33) + frame(3, 30, -131071)))
    assert result['frames'] == 3
    assert result['unique_observed']['0'] == 2  # same slot, different generation
    assert result['rewinds'] == [dict(wall=3, before=33, after=30)]


def test_partial_shutdown_frame_is_flagged_and_not_counted():
    result = summarize(io.StringIO(frame(1, 30) + row(2, 'world_begin', 33, 1, 1, 0, 1)))
    assert result['frames'] == 1
    assert result['events'][-1]['kind'] == 'incomplete_final_frame'


def test_missing_middle_boundary_is_rejected():
    text = row(1, 'world_begin', 30) + row(2, 'world_begin', 33)
    with pytest.raises(ValueError, match='Missing world_end'):
        list(frames(records(io.StringIO(text)), []))


@pytest.mark.parametrize('text', ['1,world_begin,3\n', row(1, 'world_begin', float('nan'))])
def test_malformed_rows_cannot_count_as_evidence(text):
    with pytest.raises(ValueError):
        list(records(io.StringIO(text)))


def test_structure_collision_does_not_expose_undefined_object_union():
    result = summarize(io.StringIO(frame(1, 30)))
    assert result['samples'][0]['players']['-100']['floor'] == [1, 2, -1, 3]
    result = summarize(io.StringIO(frame(1, 30, floor_type=3)))
    assert result['samples'][0]['players']['-100']['floor'][2] == 777


def test_missing_device_is_absent_not_open():
    result = summarize(io.StringIO(frame(1, 30)), selected_name=10)
    assert result['samples'][0]['device'] is None


def test_missing_unit_fields_rejected():
    text = '\n'.join(line for line in frame(1, 30).splitlines() if 'world_control' not in line)
    with pytest.raises(ValueError, match='Missing unit state'):
        summarize(io.StringIO(text))
