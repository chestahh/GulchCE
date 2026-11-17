"""Exercise real shader selection/caching and GPU guards with counted GL calls."""
import pytest
from tools.harness import CHECK_FAILED, build, function, mutated, read, run, structure


def executable(android=False, fault=None):
    renderer = read('port/linux/src/d3d8_gl.c')
    header = read('port/linux/src/xgpu.h')
    code = '#define HALO_ANDROID 1\n' if android else ''
    code += '\n'.join(structure(header, n) for n in ('nv2a_vertex_lighting', 'nv2a_pixel_shader_key'))
    code += '\n' + structure(renderer, 'vertex_element')
    code += '\n' + structure(renderer, 'vertex_shader_object')
    code += '\nstatic struct { struct vertex_shader_object *vertex_shader; } device;\n'
    code += '\n'.join(function(renderer, n) for n in ('vertex_shader_get', 'draw_program_get'))
    # Take the integration blocks verbatim, including their platform and
    # activation guards. The rest of prepare_draw concerns stock GPU state.
    init = function(renderer, 'program_get')
    init = init[init.index('#ifndef HALO_ANDROID\n\tentry->plasma_model_slots'):]
    init = init[:init.index('\n\tfor (stage =')]
    draw = function(renderer, 'prepare_draw')
    draw = draw[draw.index('#ifndef HALO_ANDROID\n\tif (plasma_mask &&'):]
    draw = draw[:draw.index('\n\t/* the lights of a draw')]
    code += '\nstatic void initialize(struct program_entry *entry, BOOL plasma_model) {\n' + init + '\n}\n'
    code += '\nstatic void bind_model(struct program_entry *entry, unsigned int plasma_mask) {\n' + draw + '\n}\n'
    if fault:
        code = mutated(code, *fault)
    return build('dynamic_shader_isolation', (('under_test.inc', code),))


@pytest.mark.parametrize('android', [False, True])
@pytest.mark.parametrize('case', ['toggle', 'failure', 'unmarked', 'gpu'])
def test_isolation(case, android):
    status, output = run(executable(android), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    (('plasma_mask && program->plasma_model', 'program->plasma_model'), 'toggle'),
    (('failed = &program->plasma_lighting_failed;', 'failed = &program->lighting_failed;'), 'failure'),
    (('if (plasma_model)\n\t{\n\t\tentry->plasma_model_slots', 'if (TRUE)\n\t{\n\t\tentry->plasma_model_slots'), 'gpu'),
    (('if (plasma_mask && entry->plasma_model_slots >= 0)', 'if (entry->plasma_model_slots >= 0)'), 'gpu'),
])
def test_negative_control(fault, case):
    status, output = run(executable(fault=fault), case)
    assert status == CHECK_FAILED, output
