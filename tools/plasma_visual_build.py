"""Build a capture-only executable, restoring normal source even on failure.

Run from the repository with the Windows build environment configured.
Normal builds never include the fixture or its simulation hooks.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import re
import shlex
import json
import hashlib
import argparse
from lighting_trace_check import instrument, instrument_collision
from harness import function

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT/'build/plasma-visual')
parser.add_argument('--vehicle-timing', action='store_true', help='Buffered timings for the normal vehicle muzzle renderer; no multi-source prototype')
parser.add_argument('--turret-prototype', action='store_true', help='Capture-only one/two/four-source turret visibility comparison')
parser.add_argument('--verify-traces', action='store_true')
parser.add_argument('--shadow-reuse', action='store_true', help='Measure conservative cross-frame shadow reuse; diagnostic overhead only')
parser.add_argument('--shadow-geometry', action='store_true', help='Dump actual shadow meshes and poses for geometry inspection; capture build only')
parser.add_argument('--attachment-diagnostics', action='store_true', help='Log authored light eligibility and private host fixture admission; capture only')
parser.add_argument('--defer-restored-build', action='store_true', help='Nested builders only: restore sources now; outer builder must rebuild after restoring its own inputs')
parser.add_argument('--near-source-ticks', action='store_true', help='Dump geometry on grenade fixture ticks 115 and 130')
args = parser.parse_args()
source = ROOT / 'port/linux/game/render_interpolation.c'
fixture = ROOT / 'port/linux/game/plasma_capture_fixture.c'
original = source.read_bytes()
shadow = ROOT / 'port/linux/game/modern_shadows.c'
shadow_original = shadow.read_bytes()
shadow_gl = ROOT/'port/linux/src/modern_shadows_gl.c'
shadow_gl_original = shadow_gl.read_bytes()
shadow_header = ROOT/'port/linux/include/modern_shadows.h'
shadow_header_original = shadow_header.read_bytes()
main = ROOT / 'source/main/main.c'
backend = ROOT / 'port/linux/src/d3d8_gl.c'
main_original = main.read_bytes()
backend_original = backend.read_bytes()
models = ROOT/'source/models/models.c'
models_original = models.read_bytes()
plasma = ROOT / 'port/linux/game/plasma_lighting.c'
plasma_original = plasma.read_bytes()
environment = ROOT/'source/rasterizer/xbox/rasterizer_xbox_environment.c'
environment_original = environment.read_bytes()
projectiles = ROOT/'source/items/projectiles.c'
projectiles_original = projectiles.read_bytes()
occlusion = ROOT/'port/linux/game/plasma_occlusion.c'
occlusion_original = occlusion.read_bytes()
collision=ROOT/'port/linux/game/plasma_collision.c'
collision_original=collision.read_bytes()
collision_models=ROOT/'port/linux/game/plasma_collision_models.c'
collision_models_original=collision_models.read_bytes()
input_source=ROOT/'port/linux/src/xinput_sdl.c'
input_original=input_source.read_bytes()
rocket=ROOT/'port/linux/game/rocket_lighting.c'
rocket_original=rocket.read_bytes()
muzzle=ROOT/'port/linux/game/muzzle_lighting.c'
muzzle_original=muzzle.read_bytes()
covenant=ROOT/'port/linux/game/covenant_lighting.c'
covenant_original=covenant.read_bytes()
emitter=ROOT/'port/linux/game/covenant_emitter_lighting.c'
emitter_original=emitter.read_bytes()
shield=ROOT/'port/linux/game/shield_glow.c'
shield_original=shield.read_bytes()
light_source=ROOT/'source/objects/object_lights.c'
light_original=light_source.read_bytes()
network_source=ROOT/'port/linux/game/network_test.c'
network_original=network_source.read_bytes()
assert not fixture.exists(), 'Refusing to overwrite an existing fixture'
out = args.output_dir.resolve()
assert out.is_relative_to((ROOT/'build').resolve()), 'Capture output must stay under build/'
out.mkdir(parents=True, exist_ok=True)
saved_args = shlex.split(re.search(r'^configure_args = (.*)$',
                                  (ROOT / 'build.ninja').read_text().replace('$\n', ''), re.M).group(1))
manifest = dict(configure_args=saved_args, base_commit=subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), source_hashes={})
for path in [ROOT/'port/linux/game/plasma_collision_scene.c', ROOT/'port/linux/game/lighting_config.c', ROOT/'source/physics/collision_bsp.c', ROOT/'source/physics/collision_bsp.h', ROOT/'source/physics/collision_models.c', ROOT/'source/physics/collision_models.h', ROOT/'port/linux/game/ambient_emitter_lighting.c', ROOT/'port/linux/game/ambient_emitter_lighting.h', ROOT/'port/linux/game/lighting_menu.c', ROOT/'port/linux/game/authored_emitter_lighting.c', ROOT/'port/linux/game/authored_emitter_lighting.h', ROOT/'source/rasterizer/xbox/rasterizer_xbox_transparent_geometry.c', ROOT/'port/linux/game/needler_lighting.c', ROOT/'port/linux/game/ammo_counter_falloff.c', ROOT/'source/rasterizer/xbox/rasterizer_xbox_models.c', ROOT/'port/linux/game/ammo_counter_lighting.c', covenant, muzzle, ROOT/'source/items/weapons.c', ROOT/'port/linux/src/port_config.c', rocket, input_source, ROOT/'tools/fixtures/plasma_lighting_capture.c', source, main, backend, plasma, ROOT/'port/linux/game/plasma_falloff.c',
             occlusion, ROOT/'port/linux/src/plasma_occlusion_gl.c', ROOT/'source/objects/object_lights.c', ROOT/'source/rasterizer/xbox/rasterizer_xbox.c',
             ROOT/'port/linux/src/plasma_model_shader.c', ROOT/'port/linux/src/nv2a_vsh.c', ROOT/'port/linux/src/nv2a_psh.c', ROOT/'source/rasterizer/xbox/rasterizer_xbox_environment.c']:
    manifest['source_hashes'][path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
game = ROOT/'source/game/game.c'
game_original = game.read_bytes()
try:
    load_draw = 'material_prepare_map();'
    game_text = game.read_text()
    assert game_text.count(load_draw) == 1
    game.write_text('void platform_log(const char *, ...);\n' + game_text.replace(load_draw,
        '{ unsigned long start = GetTickCount(); material_prepare_map(); '
        'platform_log("SHADOW LOAD materials %lu ms", GetTickCount() - start); }'))
    manifest['attachment_diagnostics'] = args.attachment_diagnostics
    if args.attachment_diagnostics:
        manifest['source_hashes'][network_source.relative_to(ROOT).as_posix()] = hashlib.sha256(network_original).hexdigest()
        text = light_original.decode().replace('\r\n', '\n')
        marker = '\t\t\tplasma_lighting_prepare(light_index, light->object_index, light->definition_index);'
        assert text.count(marker) == 1
        text = text.replace(marker, marker + '\n' + r'''
        if (light->object_index != NONE && object_try_and_get(light->object_index) &&
            object_get(light->object_index)->object.type == _object_type_vehicle && !(game_time_get() % 30)) {
            struct point_light_definition *d = light_definition_get(light->definition_index);
            real value = 0; object_get_function_value(light->object_index, light->function_index, &value);
            platform_log("SHADOW ATTACH %s owner %ld flags %u dynamic %d connected %d radius %.6f tag_radius %.6f modifiers %.6f %.6f function %.6f colours %.4f %.4f %.4f / %.4f %.4f %.4f",
                tag_get_name(light->definition_index), light->object_index, light->flags,
                TEST_FLAG(light->flags,_point_light_dynamic_bit), TEST_FLAG(light->flags,_point_light_connected_to_map_bit),
                light->radius, d->radius, d->radius_modifier_lower_bound,d->radius_modifier_upper_bound,value,
                d->color_lower_bound.rgb.red,d->color_lower_bound.rgb.green,d->color_lower_bound.rgb.blue,
                d->color_upper_bound.rgb.red,d->color_upper_bound.rgb.green,d->color_upper_bound.rgb.blue);
        }
''')
        light_source.write_text(text)
        text = network_original.decode().replace('\r\n', '\n')
        marker = 'network_test.player_added = network_game_client_add_player(global_network_game_client_get(), 0);'
        assert text.count(marker) == 1
        text = text.replace(marker, marker + '\n\t\t\t\tplatform_log("SHADOW HOST player added %d", network_test.player_added);')
        network_source.write_text(text)
    manifest['source_hashes'][collision.relative_to(ROOT).as_posix()]=hashlib.sha256(collision_original).hexdigest()
    manifest['source_hashes'][collision_models.relative_to(ROOT).as_posix()]=hashlib.sha256(collision_models_original).hexdigest()
    header=ROOT/'port/linux/include/plasma_collision.h'
    manifest['source_hashes'][header.relative_to(ROOT).as_posix()]=hashlib.sha256(header.read_bytes()).hexdigest()
    manifest['verify_traces']=args.verify_traces
    if args.verify_traces:
        for path in [ROOT/'tools/lighting_trace_check.py', ROOT/'tools/fixtures/lighting_trace_check.inc']:
            manifest['source_hashes'][path.relative_to(ROOT).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
    for path in [shield, ROOT/'port/linux/game/overshield_effect.c', ROOT/'port/linux/game/shield_look.c']:
        manifest['source_hashes'][path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    text = 'void platform_log(const char *format,...);\n' + shield_original.decode().replace('\r\n','\n')
    marker = '\t\tcolor.blue = sample.color.blue * glow->strength;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\tplatform_log("SHIELD light time %.6f unit %ld strength %.5f position %.5f %.5f %.5f radius %.5f color %.5f %.5f %.5f", now, sample.unit_index, glow->strength, sample.position.x, sample.position.y, sample.position.z, sample.radius, color.red, color.green, color.blue);')
    shield.write_text(text)
    manifest['source_hashes'][emitter.relative_to(ROOT).as_posix()] = hashlib.sha256(emitter_original).hexdigest()
    text = 'void platform_log(const char *format,...);\n' + emitter_original.decode().replace('\r\n','\n')
    marker = '\t\temitter->connected = TRUE;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\tplatform_log("EMITTER sample time %.6f strength %.6f kind %d position %.5f %.5f %.5f color %.5f %.5f %.5f", now, emitter->strength, emitter->kind, sample.position.x, sample.position.y, sample.position.z, color.red, color.green, color.blue);')
    if args.shadow_geometry:
        marker = '\t\tsample->position = marker.matrix.position;'
        text = text.replace(marker, marker+'\n\t\t{ static int seen; if(!seen++) platform_log("SHADOW EMITTER axes forward %.6f %.6f %.6f up %.6f %.6f %.6f", marker.matrix.forward.i,marker.matrix.forward.j,marker.matrix.forward.k,marker.matrix.up.i,marker.matrix.up.j,marker.matrix.up.k); }')
    emitter.write_text(text)
    text='int plasma_capture_skip_cinematic(void);\nint plasma_capture_turret_fire(void);\nint plasma_capture_scorpion_fire(void);\n'+input_original.decode().replace('\r\n','\n')
    marker='\tif (seed < 0)'
    assert marker in text
    text=text.replace(marker,'\tif (getenv("HALO_PLASMA_CAPTURE_DOOR")) { pad->bAnalogButtons[XINPUT_GAMEPAD_A] = SDL_GetTicks()%1000 < 100 ? 255 : 0; return; }\n\tif (getenv("HALO_PLASMA_CAPTURE_COMBAT")) { pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = (getenv("HALO_PLASMA_CAPTURE_KIND") && !strcmp(getenv("HALO_PLASMA_CAPTURE_KIND"), "pistol") ? SDL_GetTicks()%400 < 100 : SDL_GetTicks()%3000 < 2200) ? 255 : 0; return; }\n'+marker,1)
    text = text.replace(marker, '\tif (getenv("HALO_MUZZLE_CAPTURE_WEAPON") && !getenv("HALO_MUZZLE_CAPTURE_CHARGED")) { pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = SDL_GetTicks()%900 < 100 ? 255 : 0; return; }\n'+marker,1)
    # Muzzle input must precede the general combat-fixture input.
    start = text.index('\tif (getenv("HALO_MUZZLE_CAPTURE_WEAPON")')
    end = text.index('\n', start) + 1
    line = text[start:end]
    text = text[:start]+text[end:]
    at = text.index('\tif (getenv("HALO_PLASMA_CAPTURE_COMBAT"))')
    text = text[:at]+line+text[at:]
    at = text.index('\tif (getenv("HALO_PLASMA_CAPTURE_COMBAT"))')
    text = text[:at]+'\tif (getenv("HALO_WARTHOG_CAPTURE_EXTERIOR")) { pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = plasma_capture_turret_fire() ? 255 : 0; return; }\n'+text[at:]
    at = text.index('\tif (getenv("HALO_PLASMA_CAPTURE_COMBAT"))')
    text = text[:at]+'\tif (getenv("HALO_SCORPION_CAPTURE")) { int fire = plasma_capture_scorpion_fire(); pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = fire & 1 ? 255 : 0; pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] = fire & 2 ? 255 : 0; return; }\n'+text[at:]
    text = text.replace(marker, '\tif (getenv("HALO_COVENANT_CAPTURE_CAMPAIGN") && plasma_capture_skip_cinematic()) pad->bAnalogButtons[XINPUT_GAMEPAD_A] = SDL_GetTicks()%1000 < 100 ? 255 : 0;\n'+marker,1)
    at = text.index('\tif (getenv("HALO_MUZZLE_CAPTURE_WEAPON")')
    text = text[:at] + '\tif (getenv("HALO_LIGHTING_CAPTURE_TURN")) { pad->sThumbRX = (SHORT)(sin(SDL_GetTicks()*0.0007)*22000); pad->sThumbRY = (SHORT)(sin(SDL_GetTicks()*0.0004)*16000); }\n' + text[at:]
    input_source.write_text(text)
    shadow_fixture = ROOT/'tools/fixtures/modern_shadow_capture.inc'
    manifest['source_hashes'][shadow_fixture.relative_to(ROOT).as_posix()] = hashlib.sha256(shadow_fixture.read_bytes()).hexdigest()
    manifest['shadow_geometry'] = args.shadow_geometry
    fixture.write_text((ROOT/'tools/fixtures/plasma_lighting_capture.c').read_text().replace(
        '/* MODERN_SHADOW_CAPTURE_FIXTURE */', shadow_fixture.read_text()))
    text = original.decode().replace('\r\n', '\n')
    text = 'struct observer_result;\nstruct observer_result const *plasma_capture_view(struct observer_result const *source);\n' + text
    camera = 'view = render_interpolation_direct_camera(local_player_index,\n\t\trender_interpolation_blended_camera(local_player_index, observer));'
    assert text.count(camera) == 1
    text = text.replace(camera, 'view = plasma_capture_view(render_interpolation_direct_camera(local_player_index,\n\t\trender_interpolation_blended_camera(local_player_index, observer)));')
    text = text.replace('void render_interpolation_tick(void)',
                        'void plasma_capture_tick(void);\nvoid plasma_capture_frame(void);\n\nvoid render_interpolation_tick(void)', 1)
    text = text.replace('long previous_tick = interpolation_tick++;',
                        'long previous_tick = interpolation_tick++;\n\tplasma_capture_tick();', 1)
    source.write_text(text)
    text = projectiles_original.decode().replace('\r\n','\n')
    text = 'int plasma_capture_hold_projectile(long index);\n'+text
    begin = text.index('boolean projectile_update(')
    marker = '\tprojectile = projectile_get(projectile_index);'
    at = text.index(marker, begin)
    text = text[:at]+'\tif (plasma_capture_hold_projectile(projectile_index)) return TRUE;\n'+text[at:]
    projectiles.write_text(text)
    text = occlusion_original.decode().replace('\r\n','\n')
    text = 'void platform_log(const char *format,...);\n'+text
    marker = '\tfield->valid = 1;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\tplatform_log("PLASMA occlusion extent %.3f sample %.3f",field->extent,field->visibility[0]);')
    occlusion.write_text(instrument(text) if args.verify_traces else text)
    if args.verify_traces:
        collision.write_text(instrument_collision(collision.read_text()))
        collision_models.write_text(instrument_collision(collision_models.read_text()))
    text = main_original.decode().replace('\r\n', '\n')
    text = 'struct render_camera;\nvoid modern_capture_camera(struct render_camera *);\nvoid plasma_capture_frame(void);\nvoid plasma_capture_camera(void);\n' + text
    marker = '\twindow->rasterizer_camera.mirrored = FALSE;'
    assert text.count(marker) == 1
    text = text.replace(marker, '\tif (observer) modern_capture_camera(&window->rasterizer_camera);\n' + marker)
    text = text.replace('main_game_render((double)main_globals.seconds_elapsed);',
                        'plasma_capture_camera();\n\t\t\t\t\tmain_game_render((double)main_globals.seconds_elapsed);\n\t\t\t\t\tplasma_capture_frame();', 1)
    main.write_text(text)
    text = backend_original.decode().replace('\r\n', '\n')
    text = 'int plasma_capture_should_capture(void);\nvoid plasma_capture_model_draw(unsigned int mask);\n' + text
    marker = '\tentry = draw_program_get(program, immediate, &key, plasma_mask);'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\tplasma_capture_model_draw(plasma_mask);', 1)
    text = text.replace('if (screenshot_every > 0 && device.frame % (unsigned long)screenshot_every == 0)',
                        'if (plasma_capture_should_capture())', 1)
    backend.write_text(text)
    text = models_original.decode().replace('\r\n', '\n')
    marker = '\t\t\trasterizer_model_begin(&model_parameters, FALSE);'
    assert text.count(marker) == 1
    text = 'void modern_capture_receiver(long);\n' + text.replace(marker,
        '\t\t\tmodern_capture_receiver(unique_identifier);\n' + marker)
    marker = '\t\t\trasterizer_model_end();'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\n\t\t\tmodern_capture_receiver(NONE);')
    if args.shadow_geometry:
        text = 'void modern_capture_visible_pose(long, const void *, int);\n' + text
        marker = '\t\t\tmodern_capture_receiver(unique_identifier);'
        text = text.replace(marker, marker + '\n\t\t\tmodern_capture_visible_pose(unique_identifier, relative_node_matrices, node_count);')
    models.write_text(text)
    text = backend.read_text()
    marker = '\tplasma_capture_model_draw(plasma_mask);'
    text = 'void modern_capture_shader(unsigned long, int, int, int, int, unsigned int);\n' + text.replace(marker,
        marker + '\n\tmodern_capture_shader(program->id, program->plasma_model, program->lighting.lights, key.per_pixel_lighting, key.modern_shadow, plasma_mask);')
    backend.write_text(text)
    text = environment_original.decode().replace('\r\n', '\n')
    text = 'void plasma_capture_specular_begin(long index);\nvoid plasma_capture_specular_draw(void);\n' + text
    text = text.replace('void _rasterizer_environment_specular_light_begin(\n\tlong light_index)\n{', 'void _rasterizer_environment_specular_light_begin(\n\tlong light_index)\n{\n\tplasma_capture_specular_begin(light_index);', 1)
    start = text.index('void _rasterizer_environment_specular_light_draw(')
    end = text.index('void _rasterizer_environment_specular_lights_end(', start)
    part = text[start:end]
    marker = '\t\trasterizer_draw_dynamic_triangles_static_vertices('
    assert marker in part
    part = part.replace(marker, '\t\tplasma_capture_specular_draw();\n' + marker)
    text = text[:start] + part + text[end:]
    environment.write_text(text)
    text = plasma_original.decode().replace('\r\n', '\n')
    text = 'void platform_log(const char *format, ...);\n' + text
    marker = '\tif (!light_port_refresh_attachment(light_index, object_index))'
    assert text.count(marker) == 1
    text = text.replace(marker, '\tplatform_log("PLASMA prepare %ld owner %ld tag %s", light_index, object_index, name);\n' + marker)
    text = text.replace('plasma_lighting_attachments[index].rasterizer_index = rasterizer_index;',
                        '{ plasma_lighting_attachments[index].rasterizer_index = rasterizer_index; platform_log("PLASMA submit %ld", rasterizer_index); }', 1)
    plasma.write_text(text)
    text = rocket_original.decode().replace('\r\n', '\n')
    text = 'void platform_log(const char *format,...);\n' + text
    marker = '\t\t\trocket_lights[i].rasterizer_index = rasterizer_index;'
    assert marker in text
    text = text.replace(marker, marker + '\n\t\t\tplatform_log("PLASMA prepare %ld owner %ld tag weapons\\\\rocket launcher\\\\rocket", light_index, rocket_lights[i].object_index);')
    rocket.write_text(text)
    text = covenant_original.decode().replace('\r\n','\n')
    text = 'void platform_log(const char *format,...);\n' + text
    marker = '\t\t\tcovenant_lights[i].rasterizer_index = rasterizer_index;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\n\t\t\tplatform_log("PLASMA prepare %ld owner %ld tag %s", light_index, covenant_lights[i].object_index, covenant_profiles[covenant_lights[i].profile].tag);')
    covenant.write_text(text)
    text = muzzle_original.decode().replace('\r\n','\n')
    text = '#include <stdlib.h>\nvoid platform_log(const char *format,...);\n' + text
    # Source-placement comparisons keep visible flashes and all casters intact.
    marker = '\tif (profile->forward_offset != 0.0f)'
    assert text.count(marker) == 1
    text = text.replace(marker, '\tif (profile->vehicle_occlusion && getenv("HALO_MUZZLE_CAPTURE_OFFSET"))\n'
        '\t{\n\t\treal delta = (real)atof(getenv("HALO_MUZZLE_CAPTURE_OFFSET")) - profile->forward_offset;\n'
        '\t\tposition->x += forward.i * delta; position->y += forward.j * delta; position->z += forward.k * delta;\n\t}\n' + marker)
    marker = '\t\tpulse->connected = TRUE;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\tplatform_log("MUZZLE sample tick %ld time %.6f strength %.6f position %.5f %.5f %.5f weapon %ld trigger %d", game_time_get(), now, strength, position.x, position.y, position.z, pulse->weapon, pulse->trigger);\n\t\tplatform_log("MUZZLE profile %s", muzzle_profiles[pulse->profile].tag);')
    marker = '\t\t\tpulses[i].rasterizer = rasterizer_index;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\t\tplatform_log("MUZZLE submit %ld", rasterizer_index);')
    muzzle.write_text(text)
    text = 'int modern_capture_skip_caster(long);\nvoid modern_capture_caster(long, int);\n' + shadow_original.decode().replace('\r\n', '\n')
    marker = '\t\tif (!model_caster(model, object_get_node_matrices(iterator.index)'
    assert text.count(marker) == 1
    text = text.replace(marker, '\t\tif (modern_capture_skip_caster(iterator.index)) continue;\n\t\tmodern_capture_caster(iterator.index, views);\n' + marker)
    if args.shadow_geometry:
        text = text.replace('#include "models/model_definitions.h"', '#include "models/model_definitions.h"\n#include "tag_files/tag_files.h"')
        text = ('int modern_capture_surface_object(long);\nvoid modern_capture_surface(int,const float*,int);\n'
                'void modern_capture_shadow_pose(long,const float*,int);\n'
                'static long capture_object = -1;\nstatic int capture_surface;\n') + text
        marker = '\tfor (region = 0; region < MIN(model->regions.count, MAXIMUM_REGIONS_PER_OBJECT); region++)'
        assert text.count(marker) == 1
        text = text.replace(marker, '\tmodern_capture_shadow_pose(capture_object, matrices, model->nodes.count);\n' + marker)
        marker = '\t\tif (!model_caster(model, object_get_node_matrices(iterator.index)'
        text = text.replace(marker, '\t\tcapture_object = iterator.index;\n' + marker)
        marker = '\t\tif (!model_caster(model_definition_get(model_index), nodes,'
        text = text.replace(marker, '\t\tcapture_object = NONE;\n' + marker)
        marker = '\t\tobject_function_values_for_render(iterator.index, values);'
        text = text.replace(marker, '\t\tcapture_surface = modern_capture_surface_object(iterator.index);\n' + marker)
        marker = '\t\t\tif (!type) continue;'
        model_start = text.index('static int model_caster(')
        text = text[:model_start] + text[model_start:].replace(marker, '\t\t\tif (!type) { if(capture_surface) { int surface = mesh_get(&p->vertex_buffer, &p->triangle_buffer, &p->triangle_buffer, NULL, 0, model->nodes.count);\n'
            '\t\t\tif(surface != NONE) { platform_log("SHADOW SURFACE mesh %d shader %s",parts[surface].mesh,tag_get_name(reference->shader.index)); modern_capture_surface(parts[surface].mesh,matrices,model->nodes.count); } } continue; }', 1)
        marker = '\t\tobject_function_values_for_render(iterator.index, values);'
        text = text.replace(marker, '\t\t{ static long seen[512]; static int count; int j; for(j=0;j<count;j++) if(seen[j]==iterator.index) break;\n'
            '\t\tif(j==count && count<512) { seen[count++]=iterator.index; platform_log("SHADOW OBJECT %ld model %p tag %s",iterator.index,(void *)model,tag_get_name(object->definition_index)); } }\n'+marker)
        marker = '\t\t\tif (part == NONE || !modern_shadow_draw('
        text = text.replace(marker, '\t\t\t{ static int seen[2048]; if(part>=0 && part<2048 && !seen[part]) { seen[part]=1; platform_log("SHADOW PART model %p part %d mesh %d region %d geometry %d shader %s",(void *)model,part,parts[part].mesh,region,geometry_index,tag_get_name(reference->shader.index)); } }\n'+marker)
    marker = 'active |= 1 << slot;'
    assert text.count(marker) == 1
    text = 'void platform_log(const char *, ...);\n' + text.replace(marker, marker +
        '\n\t\t\t{ static int frames[MODERN_SHADOW_LIGHTS]; if (!(frames[slot]++ % 30)) platform_log("SHADOW selected %ld complete %d active %d parts %d view_weapon %ld view_hands %ld slot %d", index, complete, (active >> slot) & 1, part_count, owners[0], owners[1], slot); }')
    marker = 'prepare_pending = map_mode != 0;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\n\tif (getenv("HALO_CAPTURE_SKIP_SHADOW_PREP")) prepare_pending = 0;')
    shadow.write_text(text)
    text = shadow_gl_original.decode().replace('\r\n', '\n')
    if args.shadow_geometry:
        text = '#include <stdio.h>\n' + text
        text += '\nvoid modern_capture_surface(int mesh,const float *matrices,int nodes) { FILE *f=fopen("shadow-surfaces.bin","ab"); if(f) { int index=mesh-1; fwrite(&index,4,1,f); fwrite(&nodes,4,1,f); fwrite(matrices,4,1024,f); fclose(f); } }\n'
        marker = '\tsize = vertex_count * sizeof(*vertices) + index_count * sizeof(*indices);'
        text = text.replace(marker, '\t{ FILE *f=fopen("shadow-meshes.bin",mesh_count?"ab":"wb"); if(f) {\n'
            '\t\tfwrite(&mesh_count,4,1,f); fwrite(&vertex_count,4,1,f); fwrite(&index_count,4,1,f);\n'
            '\t\tfwrite(vertices,sizeof(*vertices),vertex_count,f); fwrite(indices,2,index_count,f); fclose(f); } }\n'+marker)
        marker = '\tmap->view_map = 0;'
        selector = ('extern long near_capture_tick(void); static long seen[MODERN_SHADOW_LIGHTS]; '
                    'int frame=(int)near_capture_tick(); if((frame==115 || frame==130) && seen[map-maps]!=frame) { '
                    'seen[map-maps]=frame;' if args.near_source_ticks else
                    'static int frame; if(map==maps) frame++; if(frame==120 || frame==180) {')
        text = text.replace(marker, '\t{ '+selector+'\n'
            '\t\tchar name[80]; FILE *f; int slot=(int)(map-maps); snprintf(name,sizeof(name),"shadow-draws-%d-%d.bin",frame,slot); f=fopen(name,"wb");\n'
            # Keep the retained geometry-dump layout independent of the new
            # transient face mask in the production queue.
            '\t\tif(f) { int d; fwrite(map->light,4,4,f); fwrite(&draw_count,4,1,f);\n'
            '\t\tfor(d=0;d<draw_count;d++) { fwrite(&draws[d].mesh,4,3,f);\n'
            '\t\tfwrite(draws[d].matrices,sizeof(draws[d].matrices),1,f);\n'
            '\t\tfwrite(draws[d].uv_transform,sizeof(draws[d].uv_transform),1,f); fwrite(&draws[d].alpha,4,1,f); } fclose(f); } } }\n'+marker)
    # Fault injection stays in the capture executable. Exercise the real sticky
    # resource-failure path and its recovery after an Off/On settings cycle.
    marker = '\tif (program) return 1;'
    assert text.count(marker) == 1
    text = '#include <stdlib.h>\n' + text.replace(marker,
        '\t{ static int injected; if (!injected && getenv("HALO_SHADOW_CAPTURE_RESOURCE_FAILURE")) {\n'
        '\t\tinjected = 1; platform_log("SHADOW TEST resource failure injected"); return 0;\n'
        '\t} }\n' + marker)
    manifest['shadow_reuse'] = args.shadow_reuse
    if args.shadow_reuse:
        probe = (ROOT/'tools/fixtures/modern_shadow_reuse_probe.inc').read_text()
        manifest['shadow_reuse_probe_sha256'] = hashlib.sha256(probe.encode()).hexdigest()
        text = text.replace('int modern_shadow_end(void)', probe+'\nint modern_shadow_end(void)', 1)
    marker = '\t\tmap->valid = 1;'
    assert text.count(marker) == 1
    if args.shadow_reuse:
        text = text.replace(marker, marker + '\n\t\tmodern_shadow_reuse_probe();')
    text = text.replace(marker, marker + '''
        { static int frames; if (!(frames++ % 30)) {
            int count = 0, reference = 0, d, p, f;
            for (p = 0; p < 1 + map->view_map; p++)
            for (d = 0; d < draw_count; d++) if (draws[d].views & (1 << p)) {
                reference += 6;
                for (f = 0; f < 6; f++) count += !!(draws[d].faces & (1 << f));
            }
            platform_log("SHADOW MAPS views %d draws %d faces %d uncropped_faces %d",
                1 + map->view_map, draw_count, count, reference);
        } }
''')
    body = function(text, 'modern_shadow_dispose')
    logged = body.replace('{', '{\n\tint i, freed=0, remaining=0;\n\tfor(i=0;i<MODERN_SHADOW_LIGHTS;i++) freed+=!!maps[i].depth_cube[0]+!!maps[i].depth_cube[1];', 1)
    logged = logged.replace('framebuffer = program = vao = alpha_sampler = 0;', 'framebuffer = program = vao = alpha_sampler = 0;\n\tfor(i=0;i<MODERN_SHADOW_LIGHTS;i++) remaining+=!!maps[i].depth_cube[0]+!!maps[i].depth_cube[1];\n\tplatform_log("SHADOW FREE cubes %d remaining %d meshes %d", freed, remaining, mesh_count);')
    text = text.replace(body, logged)
    shadow_gl.write_text(text)
    manifest['source_hashes']['port/linux/game/modern_shadows.c'] = hashlib.sha256(shadow_original).hexdigest()
    for name in ['port/linux/src/modern_shadows_gl.c', 'port/linux/include/modern_shadows.h', 'source/render/render.c',
                 'port/linux/game/modern_shadows_first_person.inl', 'source/interface/first_person_weapons.c',
                 'source/rasterizer/xbox/rasterizer_xbox_models.c']:
        manifest['source_hashes'][name] = hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
    manifest['source_hashes']['port/linux/src/modern_shadows_gl.c'] = hashlib.sha256(shadow_gl_original).hexdigest()
    manifest['source_hashes'][models.relative_to(ROOT).as_posix()] = hashlib.sha256(models_original).hexdigest()
    if args.turret_prototype or args.vehicle_timing:
        from turret_shadow_probe import install, quiet_logs
        for path in [shadow, shadow_gl, muzzle, plasma, rocket, covenant, emitter, shield, occlusion, models, fixture]:
            path.write_text(quiet_logs(path.read_text()))
        if args.turret_prototype:
            install(ROOT)
        text = fixture.read_text()
        text = text.replace('void plasma_capture_frame(void)\n{',
            'static void turret_probe_frame(void);\nvoid plasma_capture_frame(void)\n{\n'
            '\tif (getenv("HALO_TURRET_TIMING")) { turret_probe_frame(); return; }', 1)
        text = text.replace('if (capture_ticks < 75 || capture_ticks > 240) return 0;',
            'if (capture_ticks < 75 || (capture_ticks > 240 && !getenv("HALO_TURRET_TIMING"))) return 0;', 1)
        selector = ('int turret_probe_basis(long,float *,float *);\nstatic int turret_timing_selected(long index) { float axis[3],up[3]; return turret_probe_basis(index,axis,up); }\n' if args.turret_prototype else '#include \"modern_shadows.h\"\nstatic int turret_timing_selected(long index) { return modern_shadows_collision_owner(index)!=-1; }\n')
        text += '\n' + selector + (ROOT/'tools/fixtures/turret_shadow_timing.inc').read_text()
        fixture.write_text(text)
        manifest['turret_prototype'] = args.turret_prototype
        manifest['vehicle_timing'] = True
        manifest['turret_probe_hashes'] = {str(p): hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
            ['tools/turret_shadow_probe.py','tools/fixtures/turret_shadow_probe.inc','tools/fixtures/turret_shadow_timing.inc']}
    subprocess.run([sys.executable, 'configure.py', *saved_args], cwd=ROOT, check=True)
    subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
    shutil.copy2(ROOT / 'build/windows/halo.exe', out / 'halo-capture.exe')
    if (ROOT/'build/windows/halo.pdb').exists():
        shutil.copy2(ROOT/'build/windows/halo.pdb', out/'halo.pdb')
    manifest['capture_sha256'] = hashlib.sha256((out/'halo-capture.exe').read_bytes()).hexdigest()
    (out/'build-manifest.json').write_text(json.dumps(manifest, indent=2))
finally:
    game.write_bytes(game_original)
    shadow_header.write_bytes(shadow_header_original)
    if args.attachment_diagnostics:
        light_source.write_bytes(light_original)
        network_source.write_bytes(network_original)
    shadow.write_bytes(shadow_original)
    shadow_gl.write_bytes(shadow_gl_original)
    shield.write_bytes(shield_original)
    emitter.write_bytes(emitter_original)
    covenant.write_bytes(covenant_original)
    muzzle.write_bytes(muzzle_original)
    rocket.write_bytes(rocket_original)
    input_source.write_bytes(input_original)
    source.write_bytes(original)
    main.write_bytes(main_original)
    backend.write_bytes(backend_original)
    models.write_bytes(models_original)
    plasma.write_bytes(plasma_original)
    environment.write_bytes(environment_original)
    projectiles.write_bytes(projectiles_original)
    occlusion.write_bytes(occlusion_original)
    collision.write_bytes(collision_original)
    collision_models.write_bytes(collision_models_original)
    fixture.unlink(missing_ok=True)
    subprocess.run([sys.executable, 'configure.py', *saved_args], cwd=ROOT, check=True)
    if not args.defer_restored_build:
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
