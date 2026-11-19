"""Exercise actual feature-light eligibility, shadow selection and lifecycle."""
from tools.harness import CHECK_FAILED, build, constant, function, read, run


def test_shared_resolution_bounds():
    source = read('port/linux/src/d3d8_gl.c')
    code = ''.join(f'#define {name} {constant(source, name)}\n' for name in
                   ('SHADOW_MAP_SIZE', 'SHADOW_MAP_MAXIMUM_SCALE'))
    code += function(source, 'shadow_scale_choose')
    status, output = run(build('shadow_resolution', (('under_test.inc', code),)), 'sizes')
    assert status == 0, output


def test_selection_and_lifecycle():
    check_selection(2)
    check_selection(constant(read('port/linux/include/modern_shadows.h'), 'MODERN_SHADOW_LIGHTS'))


def check_selection(budget, fault=False):
    source = read('port/linux/game/modern_shadows.c')
    code = source[source.index('static int part_count;'):source.index('static int setting(void)')]
    code += function(read('port/linux/game/plasma_lighting.c'), 'plasma_lighting_selected')
    for name in ('setting', 'modern_shadows_reset', 'modern_shadows_window',
                 'modern_shadows_submitted', 'modern_shadows_model_slot',
                 'map_for', 'model_map', 'modern_shadows_map', 'modern_shadows_model_mask', 'modern_shadows_selected',
                 'modern_shadows_receiver', 'modern_shadows_view_receiver', 'modern_shadows_collision_owner',
                 'modern_shadows_cancel', 'modern_shadows_world_begin',
                 'modern_shadows_world_end', 'modern_shadows_world_stage',
                 'modern_shadows_world_extent', 'select_light', 'select_lights', 'modern_shadows_prepare'):
        code += function(source, name)
    if fault:
        code = code.replace('unshadowed_object(selections[map].unshadowed_owner, receiver_object)', 'FALSE')
    status, output = run(build('modern_shadows', (('under_test.inc', code),
                        ('budget.inc', f'#define MODERN_SHADOW_LIGHTS {budget}\n'))), 'all')
    assert status == (CHECK_FAILED if fault else 0), output


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
    code += source[source.index('enum { SHADOW_MESHES'):source.index('static void create_depth_cube')]
    for name in ('modern_shadow_begin', 'modern_shadow_draw', 'modern_shadow_end', 'modern_shadow_clear_draws', 'modern_shadow_dispose', 'modern_shadow_bind'):
        code += function(source, name)
    status, output = run(build('modern_shadow_maps', (('under_test.inc', code),)), 'maps')
    assert status == 0, output


def test_materials_and_animated_casters():
    source = read('port/linux/game/modern_shadows.c')
    code = source[source.index('struct shadow_geometry'):source.index('typedef char shadow_part_size')]
    code += function(source, 'material') + function(source, 'model_caster')
    status, output = run(build('modern_shadow_casters', (('under_test.inc', code),)), 'casters')
    assert status == 0, output


def test_player_and_npc_held_objects():
    source = read('port/linux/game/modern_shadows.c')
    code = function(source, 'unshadowed_object') + function(source, 'object_casters')
    status, output = run(build('modern_shadow_objects', (('under_test.inc', code),)), 'objects')
    assert status == 0, output


def test_owner_receiver_exemption_is_required():
    check_selection(2, fault=True)


def test_owner_caster_exemption_is_required():
    source = read('port/linux/game/modern_shadows.c')
    code = function(source, 'unshadowed_object') + function(source, 'object_casters')
    code = code.replace('unshadowed_object(unshadowed_owner, iterator.index)', 'FALSE')
    status, output = run(build('modern_shadow_objects', (('under_test.inc', code),)), 'objects')
    assert status == CHECK_FAILED, output


def test_seated_occupant_exemption_is_required():
    source = read('port/linux/game/modern_shadows.c')
    code = function(source, 'unshadowed_object') + function(source, 'object_casters')
    code = code.replace('unit->unit.parent_seat_index != NONE', 'unit->unit.parent_seat_index == NONE')
    status, output = run(build('modern_shadow_objects', (('under_test.inc', code),)), 'objects')
    assert status == CHECK_FAILED, output
