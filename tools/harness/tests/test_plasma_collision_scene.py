"""Stationary reuse must notice every collision input, without hash collisions."""
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, read, run, structure


def test_scene_snapshot():
    source = re.sub(r'^#include.*$', '', read('port/linux/game/plasma_collision_scene.c'), flags=re.M)
    source = 'enum { PLASMA_COLLISION_SCENE_BYTES = 65536 };\n' + structure(read('port/linux/include/plasma_collision.h'), 'plasma_collision_scene') + '\n' + source
    status, output = run(build('plasma_collision_scene', (('under_test.inc', source),)), 'scene')
    assert status == 0, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
