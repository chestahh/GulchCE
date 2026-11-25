"""Actual environmental owner/attachment and failure policy, independent of GL."""
from tools.harness import build, function, read, run


def test_environmental_replacement_and_fallback():
    source=read('port/linux/game/modern_shadows.c')
    code=function(source,'environmental_casters')+function(source,'modern_shadows_environment')
    status,output=run(build('environment_shadows',(('under_test.inc',code),)),'all')
    assert status==0,output


def test_sun_culling_and_surface_ownership():
    source = read('port/linux/game/modern_sun.inl')
    code = '\n'.join(function(source, fn) for fn in ('sun_reaches_view', 'sun_cluster_sky', 'sun_classify_surfaces'))
    status, output = run(build('environment_sun', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_flashlight_culls_only_outside_light_frustum():
    source = read('port/linux/game/modern_shadows.c')
    code = '\n'.join(function(source, name) for name in (
        'spot_clip_planes', 'spot_box_visible', 'spot_sphere_visible'))
    status, output = run(build('flashlight_frustum', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_gles_environmental_hooks_cannot_activate(tmp_path):
    import subprocess
    from test_mcc_identity import compile_tool
    (tmp_path / 'cseries.h').write_text('')
    (tmp_path / 'xgpu.h').write_text('')
    (tmp_path / 'modern_shadows.h').write_text(read('port/linux/include/modern_shadows.h'))
    source = tmp_path / 'check.c'
    source.write_text('#define HALO_GLES 1\n#pragma clang diagnostic ignored "-Wunused-parameter"\n' +
        read('port/linux/game/modern_shadows.c') + '\n' +
        read('port/linux/src/modern_shadows_gl.c') + r"""
int main(void) {
    float v[16] = {0};
    modern_shadows_prepare(1);
    long lights[2] = {13, 21};
    if (modern_flashlight_include(0, lights, 2, 2) != 2 || lights[0] != 13 || lights[1] != 21) return 8;
    modern_spots_project(); modern_spot_world_light(0);
    if (modern_shadows_world_stage() || modern_spot_mask()) return 4;
    modern_shadows_environment_receivers(1);
    modern_environment_receivers(1);
    if (modern_shadows_enabled() || modern_environment_marking()) return 1;
    if (modern_shadows_environment(0, 0, v, 1) || modern_environment_begin(v, 1, 1) ||
        modern_environment_end(1, v, v, v, 0)) return 2;
    modern_shadows_environment_model(0, 0);
    modern_environment_model_clear();
    if (modern_environment_model_active() || modern_environment_model_reuse() || modern_environment_model_begin(v, 1, 0)) return 3;
    modern_environment_model_end(1, 3);
    modern_shadows_reset(); modern_environment_dispose();
    return 0;
}
""")
    executable = compile_tool(tmp_path, [source])
    subprocess.run([str(executable)], check=True)


def test_environmental_model_family_and_lifecycle():
    source = read('port/linux/game/modern_shadows.c')
    code = ''.join(function(source, n) for n in ('environmental_family_bounds', 'environment_model_family', 'modern_shadows_environment_model'))
    status, output = run(build('environment_model_shadows', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_environmental_model_gpu_resource_lifecycle():
    source = read('port/linux/src/modern_environment_gl.inl')
    code = source[source.index('struct environment_receiver_uniforms'):source.index('static GLuint environment_program')]
    code += '\n'.join(function(source, n) for n in (
        'modern_environment_window', 'modern_environment_dispose',
        'modern_environment_model_clear', 'modern_environment_model_active',
        'modern_environment_model_reuse', 'environment_model_resources', 'modern_environment_model_begin',
        'modern_environment_model_end', 'sun_draw_batch', 'modern_environment_sun_static',
        'modern_environment_sun_finish', 'modern_environment_sun_model'))
    spots = read('port/linux/src/modern_spots_gl.inl')
    code += function(spots, 'flush_beam_batch') + function(spots, 'modern_spot_end')
    status, output = run(build('environment_model_maps', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_breakable_changes_invalidate_offscreen_fixture_and_sun_maps():
    code = function(read('port/linux/game/modern_shadows.c'), 'check_static_breakables')
    status, output = run(build('environment_breakables', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_scene_depth_copy_validates_new_images_and_retains_msaa_fallback():
    source = read('port/linux/src/modern_environment_gl.inl')
    state = source[source.index('struct environment_receiver_uniforms'):source.index('static GLuint environment_program')]
    code = state + function(source, 'environment_snapshot') + function(source, 'environment_draw_receiver')
    status, output = run(build('environment_snapshot', (('under_test.inc', code),)), 'all')
    assert status == 0, output


def test_world_stencil_state_and_stock_transitions():
    source = read('port/linux/src/d3d8_gl.c')
    code = '\n'.join(function(source, name) for name in
                     ('state_enable', 'stencil_operation', 'apply_stencil_state'))
    for platform in ('', '#define HALO_GLES 1\n'):
        status, output = run(build('environment_stencil', (('under_test.inc', platform + code),)), 'all')
        assert status == 0, output
