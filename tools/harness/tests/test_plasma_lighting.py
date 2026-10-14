"""Render-only attachment refresh must not survive a frame or map boundary."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run


def generated(fault=None):
    code = function(read('source/objects/object_lights.c'), 'light_port_refresh_attachment')
    code += function(read('source/objects/object_lights.c'), 'light_port_dynamic_set')
    code += function(read('source/objects/object_lights.c'), 'light_port_attachment_radius')
    code += re.sub(r'^#include.*$', '', read('port/linux/game/plasma_lighting.c'), flags=re.M)
    code += function(read('source/objects/object_lights.c'), 'light_compute_bounding_sphere')
    code += function(read('port/linux/game/render_interpolation.c'), 'render_interpolation_frame_end')
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['muzzle', 'radius', 'rocket', 'pistol', 'model', 'off', 'filter', 'restore', 'split', 'capacity', 'unload', 'missing', 'stale', 'selection', 'existing'])
def test_case(case):
    status, output = run(build('plasma_lighting', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', [
    ('strcmp(name, "weapons\\\\rocket launcher\\\\rocket")', '1', 'rocket'),
    ('strcmp(name, "weapons\\\\plasma pistol\\\\bolt") &&', '1 &&', 'pistol'),
    ('return radius * plasma_lighting_attachments[index].radius_scale;', 'return radius;', 'radius'),
    ('interpolation_rendering = FALSE;', 'interpolation_rendering = TRUE;', 'restore'),
    ('plasma_lighting_count = 0;', '(void)0;', 'unload'),
    ('!config_boolean("display.plasma_lighting") || object_index == NONE', 'object_index == NONE', 'off'),
])
def test_negative_control(before, after, case):
    status, output = run(build('plasma_lighting', generated((before, after))), case)
    assert status == CHECK_FAILED, output
