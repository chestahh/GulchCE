"""Snapshot ownership, stale replies and cleanup of the optional path worker."""
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import read, build, run


@pytest.mark.parametrize("case", ["snapshot", "stale", "cleanup", "disabled"])
def test_worker(case):
    source = read("port/linux/game/bots_worker.inl")
    code = source[source.index("struct bots_path_job"):]
    status, output = run(build("bots_worker", (("under_test.inc", code),)), case)
    assert status == 0, output
