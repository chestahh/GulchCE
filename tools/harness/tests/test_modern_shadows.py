"""Exercise actual feature-light eligibility, shadow selection and lifecycle."""
from tools.harness import build, function, read, run


def test_selection_and_lifecycle():
    source = read('port/linux/game/modern_shadows.c')
    code = source[source.index('static int part_count;'):source.index('static int setting(void)')]
    code += function(read('port/linux/game/plasma_lighting.c'), 'plasma_lighting_selected')
    for name in ('setting', 'modern_shadows_reset', 'modern_shadows_window',
                 'modern_shadows_submitted', 'modern_shadows_model_slot',
                 'modern_shadows_model_mask', 'modern_shadows_selected',
                 'modern_shadows_cancel', 'modern_shadows_world_begin',
                 'modern_shadows_world_end', 'modern_shadows_world_stage',
                 'modern_shadows_world_extent', 'modern_shadows_prepare'):
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
