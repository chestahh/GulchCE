"""Exercise the real emitter policy, render-time easing and light ownership."""
import re
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, read, run


def source():
    return function(read('source/interface/first_person_weapons.c'), 'first_person_weapon_adjust_light') + '\n' + re.sub(r'^#include.*$', '', read('port/linux/game/ambient_emitter_lighting.h') + '\n' + read('port/linux/game/needler_lighting.c') + '\n' + read('port/linux/game/ammo_counter_lighting.c') + '\n' + read('port/linux/game/covenant_emitter_lighting.c'), flags=re.M)


def test_emitters():
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', source()),)), 'emitters')
    assert status == 0, output


def test_ambient_saturation_preserves_existing_emitters():
    code = source().replace('sample.kind < EMITTER_VEHICLE && sample.strength > 0.0f', 'FALSE')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_shield_permutation_guard_is_required():
    code = source().replace('object->object.region_permutations[i] == 0', 'TRUE')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_tick_only_fade_is_detected():
    code = source().replace('render_interpolation_game_time_sec(game_time_get())', '(real)game_time_get() / 30.0f')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_first_person_counter_pose_is_required():
    code = source().replace('if (!(director_get_perspective(render.local_player_index) == _director_perspective_third_person) &&\n\t\tfirst_person_weapon_adjust_light(index, "primary trigger", position, &forward, &up))', 'if (FALSE)')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_shield_damage_colour_is_required():
    code = source().replace('PIN(object->object.outgoing_function_values[0], 0.0f, 1.0f)', '0.0f')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_needle_crystal_glow_is_required():
    code = source().replace('*output |= 0x10000UL;', '*output |= 0;')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_first_person_counter_strength_is_required():
    code = source().replace('counter_pose == 2 ? 1.35f / 0.65f : 1.0f', '1.0f')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'emitters')
    assert status == CHECK_FAILED, output


def test_camera_source_ownership():
    status, output = run(build("covenant_emitter_lighting", (("under_test.inc", source()),)), "camera")
    assert status == 0, output


def test_counter_shoulder_guard_is_required():
    code = source().replace('!(director_get_perspective(render.local_player_index) == _director_perspective_third_person) &&', 'TRUE &&')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'camera')
    assert status == CHECK_FAILED, output


def test_needler_shoulder_guard_is_required():
    code = source().replace('(director_get_perspective(render.local_player_index) == _director_perspective_third_person) ||', 'FALSE ||')
    status, output = run(build('covenant_emitter_lighting', (('under_test.inc', code),)), 'camera')
    assert status == CHECK_FAILED, output

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6
