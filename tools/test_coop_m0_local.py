"""Private fixture requests survive reader locks without hiding other errors."""
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).parent / 'investigations'))
import coop_m0_local as runner


@pytest.mark.parametrize('windows_error', [5, 32, 33, None])
def test_control_reader_lock_retry(tmp_path, monkeypatch, windows_error):
    target = tmp_path / 'input.txt'
    target.write_text('old request')
    original = Path.replace
    attempts = []
    error = PermissionError('reader lock')
    if windows_error is not None:
        error.winerror = windows_error

    def replace(path, destination):
        attempts.append(path.read_text())
        if len(attempts) == 1:
            assert destination.read_text() == 'old request'
            raise error
        return original(path, destination)

    monkeypatch.setattr(Path, 'replace', replace)
    monkeypatch.setattr(runner.time, 'sleep', lambda seconds: None)
    if windows_error is None:
        with pytest.raises(PermissionError):
            runner.write_control(target, 'complete new request\n')
        assert len(attempts) == 1
        assert target.read_text() == 'old request'
    else:
        runner.write_control(target, 'complete new request\n')
        assert attempts == ['complete new request\n'] * 2
        assert target.read_text() == 'complete new request\n'


def test_control_lock_retry_is_bounded(tmp_path, monkeypatch):
    attempts = []

    def replace(path, destination):
        attempts.append(path)
        error = PermissionError('persistent lock')
        error.winerror = 32
        raise error

    monkeypatch.setattr(Path, 'replace', replace)
    monkeypatch.setattr(runner.time, 'sleep', lambda seconds: None)
    with pytest.raises(PermissionError):
        runner.write_control(tmp_path / 'motion.txt', 'request\n')
    assert len(attempts) == 50


def test_motion_buttons_do_not_double_count_repeated_keys():
    assert runner.motion_buttons(['accept', 'jump', 'accept']) == 5
    assert runner.motion_buttons([]) == 0
    assert runner.motion_buttons(['accept', 'back', 'jump', 'crouch', 'fire']) == 31


@pytest.mark.parametrize('names', [['unknown'], 'accept'])
def test_unknown_motion_buttons_fail(names):
    with pytest.raises(ValueError):
        runner.motion_buttons(names)
