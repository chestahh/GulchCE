"""Exercise actual feature-light eligibility, shadow selection and lifecycle."""
from tools.harness import build, function, read, run


def test_selection_and_lifecycle():
    source = read('port/linux/game/modern_shadows.c')
    code = source[source.index('static int part_count;'):source.index('static int setting(void)')]
    code += function(read('port/linux/game/plasma_lighting.c'), 'plasma_lighting_selected')
    for name in ('setting', 'modern_shadows_reset', 'modern_shadows_window',
                 'modern_shadows_submitted', 'modern_shadows_model_slot',
                 'modern_shadows_model_mask', 'modern_shadows_selected',
                 'modern_shadows_receiver', 'modern_shadows_view_receiver',
                 'modern_shadows_cancel', 'modern_shadows_world_begin',
                 'modern_shadows_world_end', 'modern_shadows_world_stage',
                 'modern_shadows_world_extent', 'select_light', 'modern_shadows_prepare'):
        code += function(source, name)
    status, output = run(build('modern_shadows', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_external_pass_handoff():
    renderer = read('port/linux/src/d3d8_gl.c')
    code = function(renderer, 'xgpu_gl_state_invalidate')
    code += function(renderer, 'xgpu_gl_end_external_pass')
    draw = function(renderer, 'prepare_draw')
    fallback = draw[draw.index('\t/* A failed shadow receiver'):]
    fallback = fallback[:fallback.index('#endif')]
    code += ('\nstatic void fallback_binding(int shadow_world, int mode) {\n'
             'struct nv2a_pixel_shader_key key = {0}; struct draw_uniforms uniforms;\n'
             'key.modern_shadow = mode;\n'+fallback+'}\n')
    status, output = run(build('modern_shadow_state', (('under_test.inc', code),)), 'state')
    assert status == 0, output


def test_viewmodel_pose_and_visibility():
    code = function(read('port/linux/game/modern_shadows_first_person.inl'), 'modern_shadows_viewmodel')
    status, output = run(build('modern_shadow_viewmodel', (('under_test.inc', code),)), 'pose')
    assert status == 0, output


def test_receiver_specific_depth_maps():
    import re
    source = read('port/linux/src/modern_shadows_gl.c')
    header = read('port/linux/include/modern_shadows.h')
    code = re.sub(r'^#include.*$', '', header, flags=re.M)
    # The GL owner is tested with mocked driver calls, using its actual queue,
    # pass dispatch, lazy resource creation and receiver binding.
    code = code[code.index('struct modern_shadow_vertex'):]
    code = code[:code.index('/* Meshes are immutable')] + re.search(
        r'struct modern_shadow_uniforms\s*\{[^}]+\};', header)[0]
    code += source[source.index('enum { SHADOW_SIZE'):source.index('static void create_depth_cube')]
    for name in ('modern_shadow_draw', 'modern_shadow_end', 'modern_shadow_clear_draws', 'modern_shadow_dispose', 'modern_shadow_bind'):
        code += function(source, name)
    status, output = run(build('modern_shadow_maps', (('under_test.inc', code),)), 'maps')
    assert status == 0, output


def test_materials_and_animated_casters():
    source = read('port/linux/game/modern_shadows.c')
    code = source[source.index('struct shadow_geometry'):source.index('typedef char shadow_part_size')]
    code += function(source, 'material') + function(source, 'model_caster')
    status, output = run(build('modern_shadow_casters', (('under_test.inc', code),)), 'casters')
    assert status == 0, output
