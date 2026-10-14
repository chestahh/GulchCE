"""Exercise the actual visibility field with synthetic collision geometry."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run


def test_visibility_field():
    code = re.sub(r'^#include.*$', '', read('port/linux/game/plasma_occlusion.c'), flags=re.M)
    status, output = run(build('plasma_occlusion', (('under_test.inc', code),)), 'visibility')
    assert status == 0, output
