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

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'port/linux/game/render_interpolation.c'
fixture = ROOT / 'port/linux/game/plasma_capture_fixture.c'
original = source.read_bytes()
main = ROOT / 'source/main/main.c'
backend = ROOT / 'port/linux/src/d3d8_gl.c'
main_original = main.read_bytes()
backend_original = backend.read_bytes()
plasma = ROOT / 'port/linux/game/plasma_lighting.c'
plasma_original = plasma.read_bytes()
environment = ROOT/'source/rasterizer/xbox/rasterizer_xbox_environment.c'
environment_original = environment.read_bytes()
projectiles = ROOT/'source/items/projectiles.c'
projectiles_original = projectiles.read_bytes()
occlusion = ROOT/'port/linux/game/plasma_occlusion.c'
occlusion_original = occlusion.read_bytes()
input_source=ROOT/'port/linux/src/xinput_sdl.c'
input_original=input_source.read_bytes()
rocket=ROOT/'port/linux/game/rocket_lighting.c'
rocket_original=rocket.read_bytes()
muzzle=ROOT/'port/linux/game/muzzle_lighting.c'
muzzle_original=muzzle.read_bytes()
covenant=ROOT/'port/linux/game/covenant_lighting.c'
covenant_original=covenant.read_bytes()
assert not fixture.exists(), 'Refusing to overwrite an existing fixture'
out = ROOT / 'build/plasma-visual'
out.mkdir(exist_ok=True)
saved_args = shlex.split(re.search(r'^configure_args = (.*)$',
                                  (ROOT / 'build.ninja').read_text(), re.M).group(1))
manifest = dict(configure_args=saved_args, base_commit=subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), source_hashes={})
for path in [covenant, muzzle, ROOT/'source/items/weapons.c', ROOT/'port/linux/src/port_config.c', rocket, input_source, ROOT/'tools/fixtures/plasma_lighting_capture.c', source, main, backend, plasma, ROOT/'port/linux/game/plasma_falloff.c',
             occlusion, ROOT/'port/linux/src/plasma_occlusion_gl.c', ROOT/'source/objects/object_lights.c', ROOT/'source/rasterizer/xbox/rasterizer_xbox.c',
             ROOT/'port/linux/src/plasma_model_shader.c', ROOT/'port/linux/src/nv2a_vsh.c', ROOT/'port/linux/src/nv2a_psh.c', ROOT/'source/rasterizer/xbox/rasterizer_xbox_environment.c']:
    manifest['source_hashes'][path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
try:
    text=input_original.decode().replace('\r\n','\n')
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
    text = text.replace(marker, '\tif (getenv("HALO_COVENANT_CAPTURE_CAMPAIGN")) pad->bAnalogButtons[XINPUT_GAMEPAD_A] = SDL_GetTicks()%1000 < 100 ? 255 : 0;\n'+marker,1)
    input_source.write_text(text)
    shutil.copy2(ROOT / 'tools/fixtures/plasma_lighting_capture.c', fixture)
    text = original.decode().replace('\r\n', '\n')
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
    occlusion.write_text(text)
    text = main_original.decode().replace('\r\n', '\n')
    text = 'void plasma_capture_frame(void);\n' + text
    text = text.replace('main_game_render((double)main_globals.seconds_elapsed);',
                        'main_game_render((double)main_globals.seconds_elapsed);\n\t\t\t\t\tplasma_capture_frame();', 1)
    main.write_text(text)
    text = backend_original.decode().replace('\r\n', '\n')
    text = 'int plasma_capture_should_capture(void);\nvoid plasma_capture_model_draw(unsigned int mask);\n' + text
    text = text.replace('unsigned int mask = plasma_model_lighting_mask();', 'unsigned int mask = plasma_model_lighting_mask();\n\t\tplasma_capture_model_draw(mask);', 1)
    text = text.replace('if (screenshot_every > 0 && device.frame % (unsigned long)screenshot_every == 0)',
                        'if (plasma_capture_should_capture())', 1)
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
    marker = '\tlight_port_refresh_attachment(light_index, object_index);'
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
    text = 'void platform_log(const char *format,...);\n' + text
    marker = '\t\tpulse->connected = TRUE;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\tplatform_log("MUZZLE sample tick %ld time %.6f strength %.6f position %.5f %.5f %.5f weapon %ld", game_time_get(), now, strength, position.x, position.y, position.z, pulse->weapon);')
    marker = '\t\t\tpulses[i].rasterizer = rasterizer_index;'
    assert text.count(marker) == 1
    text = text.replace(marker, marker+'\n\t\t\tplatform_log("MUZZLE submit %ld", rasterizer_index);')
    muzzle.write_text(text)
    subprocess.run([sys.executable, 'configure.py', *saved_args], cwd=ROOT, check=True)
    subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
    shutil.copy2(ROOT / 'build/windows/halo.exe', out / 'halo-capture.exe')
    manifest['capture_sha256'] = hashlib.sha256((out/'halo-capture.exe').read_bytes()).hexdigest()
    (out/'build-manifest.json').write_text(json.dumps(manifest, indent=2))
finally:
    covenant.write_bytes(covenant_original)
    muzzle.write_bytes(muzzle_original)
    rocket.write_bytes(rocket_original)
    input_source.write_bytes(input_original)
    source.write_bytes(original)
    main.write_bytes(main_original)
    backend.write_bytes(backend_original)
    plasma.write_bytes(plasma_original)
    environment.write_bytes(environment_original)
    projectiles.write_bytes(projectiles_original)
    occlusion.write_bytes(occlusion_original)
    fixture.unlink(missing_ok=True)
    subprocess.run([sys.executable, 'configure.py', *saved_args], cwd=ROOT, check=True)
    subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
