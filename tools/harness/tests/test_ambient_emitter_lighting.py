"""Run the production ambient profiles and flame aggregation against a fake world."""
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, read, run


def source():
    return re.sub(r'^#include.*$', '', read('port/linux/game/ambient_emitter_lighting.h') + '\n' +
                  read('port/linux/game/ambient_emitter_lighting.c'), flags=re.M)


def test_ambient_emitters():
    status, output = run(build('ambient_emitter_lighting', (('under_test.inc', source()),)), 'ambient')
    assert status == 0, output


def test_disabled_world_guard_is_required():
    code = source().replace('!lighting_config_boolean(profile->kind == EMITTER_VEHICLE ? "display.vehicle_lighting" : "display.world_lighting")', 'FALSE')
    status, output = run(build('ambient_emitter_lighting', (('under_test.inc', code),)), 'ambient')
    assert status == CHECK_FAILED, output


def test_flames_are_grouped_by_owner():
    code = source().replace('flames[i].owner == object->object.owner_object_index', 'FALSE')
    status, output = run(build('ambient_emitter_lighting', (('under_test.inc', code),)), 'ambient')
    assert status == CHECK_FAILED, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6
