"""Render-only attachment refresh must not survive a frame or map boundary."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure


def generated(fault=None):
    code = function(read('source/objects/object_lights.c'), 'light_port_refresh_attachment')
    code += function(read('source/objects/object_lights.c'), 'light_port_dynamic_set')
    code += function(read('source/objects/object_lights.c'), 'light_port_attachment_radius')
    policy = read('port/linux/game/covenant_lighting.c')
    policy = policy[policy.index('static const struct covenant_profile'):policy.index('static const real_rgb_color covenant_black')]
    code += 'typedef struct { real red, green, blue; } real_rgb_color;\n' + policy
    code += enum_with(read('source/items/projectiles.c'), '_projectile_attached_bit =') + '\n'
    code += function(read('source/items/projectiles.c'), 'projectile_is_attached')
    code += re.sub(r'^#include.*$', '', read('port/linux/game/plasma_lighting.c'), flags=re.M)
    code += function(read('source/objects/object_lights.c'), 'light_compute_bounding_sphere')
    code += function(read('port/linux/game/render_interpolation.c'), 'render_interpolation_frame_end')
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code), ('grenade_shadow.inc', structure(read('port/linux/game/plasma_lighting.h'), 'plasma_shadow_source')))


@pytest.mark.parametrize('case', ['environment', 'placement', 'stuck_grenade', 'shield', 'counter_cache', 'covenant', 'muzzle', 'radius', 'rocket', 'pistol', 'model', 'off', 'filter', 'restore', 'split', 'capacity', 'unload', 'missing', 'stale', 'selection', 'disconnected', 'existing'])
def test_case(case):
    status, output = run(build('plasma_lighting', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', [
    ('strcmp(name, "weapons\\\\rocket launcher\\\\rocket")', '1', 'rocket'),
    ('strcmp(name, "weapons\\\\plasma pistol\\\\bolt") &&', '1 &&', 'pistol'),
    ('return radius * plasma_lighting_attachments[index].radius_scale;', 'return radius;', 'radius'),
    ('plasma_lighting_attachments[index].forward_offset = 0.f;', '(void)0;', 'placement'),
    ('interpolation_rendering = FALSE;', 'interpolation_rendering = TRUE;', 'restore'),
    ('plasma_lighting_count = 0;', '(void)0;', 'unload'),
])
def test_negative_control(before, after, case):
    status, output = run(build('plasma_lighting', generated((before, after))), case)
    assert status == CHECK_FAILED, output


def test_disabled_projectile_policy_is_required():
    # Authored emitters add a second category gate. Removing only the early
    # return is no longer a fault: the second gate still preserves stock.
    (name, code), grenade_header = generated()
    assert code.count('lighting_config_boolean("display.plasma_lighting")') > 1
    code = code.replace('lighting_config_boolean("display.plasma_lighting")', 'TRUE')
    status, output = run(build('plasma_lighting', ((name, code), grenade_header)), 'off')
    assert status == CHECK_FAILED, output


def test_placement_reconnect_integration():
    reconnect = function(read('source/objects/object_lights.c'), 'light_reconnect_to_map')
    hook = 'plasma_lighting_attachment_position(light_index, light->object_index,'
    assert reconnect.count(hook) == 1
    assert reconnect.index('light->position = markers[0].matrix.position;') < reconnect.index(hook)
    assert reconnect.index(hook) < reconnect.index('light_compute_bounding_sphere(')
    stub = function(read('port/android/guest/dynamic_lighting_disabled.c'), 'plasma_lighting_attachment_position')
    assert stub[stub.index('{') + 1:stub.rindex('}')].strip() == ''

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
