"""Exercise batch ownership, exact eligibility and safe fallback using real code."""
import re
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from harness import build,read,run,CHECK_FAILED

def code():
    return re.sub(r'^#include.*$','',read('port/linux/game/plasma_collision.c'),flags=re.M)

def test_depth_batch():
    status,output=run(build('plasma_collision',(('under_test.inc',code()),)),'batch')
    assert status==0,output

def test_child_gating_regression():
    broken=code().replace('object->eligible && fast_vector_intersects_sphere(batch.point, vector, &object->center, object->radius)', 'object->eligible')
    status,output=run(build('plasma_collision',(('under_test.inc',broken),)),'batch')
    assert status==CHECK_FAILED,output
