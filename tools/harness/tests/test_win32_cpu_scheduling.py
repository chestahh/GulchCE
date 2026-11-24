"""Exercise the real Windows scheduling policy with controlled CPU-set records."""
import re
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run


def test_hybrid_cpu_policy():
    source = re.sub(r'^#include.*$', '', read('port/windows/src/win32_cpu_scheduling.c'), flags=re.M)
    status, output = run(build('win32_cpu_scheduling', (('policy.inc', source),)), 'policy')
    assert status == 0, output
