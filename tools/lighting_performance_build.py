"""Build passive timing instrumentation; restore every production source afterwards."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/lighting-performance-build')
    parser.add_argument('--profile', action='store_true', help='CPU phase timers and asynchronous sampled GPU draw queries')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    assert output.is_relative_to((ROOT/'build').resolve())
    output.mkdir(parents=True, exist_ok=True)
    fixture = ROOT/'port/linux/game/lighting_performance_fixture.c'
    assert not fixture.exists()
    paths = ['source/main/main.c', 'port/linux/src/d3d8_gl.c',
             'port/linux/game/plasma_lighting.c', 'port/linux/game/plasma_occlusion.c',
             'port/linux/game/plasma_falloff.c']
    originals = {p: (ROOT/p).read_bytes() for p in paths}
    saved = shlex.split(re.search(r'^configure_args = (.*)$', (ROOT/'build.ninja').read_text(), re.M)[1])
    manifest = dict(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    configure_args=saved, production_hashes={p: hashlib.sha256(b).hexdigest() for p, b in originals.items()},
                    fixture_sha256=hashlib.sha256((ROOT/'tools/fixtures/lighting_performance.c').read_bytes()).hexdigest(),
                    profile=args.profile)
    for path in ['port/linux/src/plasma_occlusion_gl.c', 'port/linux/include/plasma_occlusion.h']:
        manifest['production_hashes'][path]=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()

    def edit(path, before, after, declaration):
        text = originals[path].decode().replace('\r\n', '\n')
        assert text.count(before) == 1, (path, before)
        (ROOT/path).write_text(declaration+'\n'+text.replace(before, after))

    try:
        shutil.copy2(ROOT/'tools/fixtures/lighting_performance.c', fixture)
        edit(paths[0], '\t\tmain_frame_rate_debug();',
             '\t\tmain_frame_rate_debug();\n\t\tif (render_frame && game_in_progress() && game_engine_running()) lighting_performance_frame();',
             'void lighting_performance_frame(void); /* port: test-build-only timing */')
        edit(paths[1], 'if (screenshot_every > 0 && device.frame % (unsigned long)screenshot_every == 0)',
             'if (lighting_performance_capture())', 'int lighting_performance_capture(void);')
        edit(paths[2], '\tshield_glow_frame_end();',
             '\tlighting_performance_lights();\n\tshield_glow_frame_end();', 'void lighting_performance_lights(void);')
        edit(paths[3], '\tupdates_this_window = 0;',
             '\tlighting_performance_occlusion(updates_this_window);\n\tupdates_this_window = 0;',
             'void lighting_performance_occlusion(int updates);')
        occlusion=ROOT/paths[3]
        text=occlusion.read_text()
        assert text.count('\tupdates_this_window++;')==1 and text.count('\tfield->valid = 1;')==1
        text=text.replace('\tupdates_this_window++;','\tupdates_this_window++;\n\tlighting_performance_occlusion_begin();')
        text=text.replace('\tfield->valid = 1;','\tfield->valid = 1;\n\tlighting_performance_occlusion_end();')
        occlusion.write_text('void lighting_performance_occlusion_begin(void);\nvoid lighting_performance_occlusion_end(void);\n'+text)
        if args.profile:
            def replace(path, before, after):
                text=path.read_text()
                assert text.count(before)==1, (path,before)
                path.write_text(text.replace(before,after))
            declarations='void lighting_profile_begin(int);\nvoid lighting_profile_end(int);\n'
            occlusion.write_text(declarations+occlusion.read_text())
            replace(occlusion, '\t\ttrace_depths(extent,', '\t\tlighting_profile_begin(0);\n\t\ttrace_depths(extent,')
            replace(occlusion, '\t\tfield->traced = now;', '\t\tlighting_profile_end(0);\n\t\tfield->traced = now;')
            replace(occlusion, '\tmemcpy(previous,', '\tlighting_profile_begin(1);\n\tmemcpy(previous,')
            replace(occlusion, '\tfield->center[0] = light->position.x;', '\tlighting_profile_end(1);\n\tfield->center[0] = light->position.x;')
            replace(occlusion, '\tscale = (pass == 2', '\tlighting_profile_begin(2);\n\tscale = (pass == 2')
            replace(occlusion, '\tfield->texture_mask |= 1 << pass;', '\tlighting_profile_end(2);\n\tfield->texture_mask |= 1 << pass;')
            backend=ROOT/paths[1]
            backend.write_text(declarations+'int lighting_profile_model, lighting_profile_world;\n'+backend.read_text())
            replace(backend, '\tif (entry->plasma_model_slots >= 0)\n\t{', '\tlighting_profile_model = entry->plasma_model_slots >= 0 && plasma_model_lighting_mask() ? 1 : 0;\n\tif (entry->plasma_model_slots >= 0)\n\t{')
            bind='\t\tplasma_occlusion_bind_model(&entry->plasma_occlusion);'
            replace(backend, bind, '\t\tlighting_profile_begin(3);\n'+bind+'\n\t\tlighting_profile_end(3);')
            gpu=(ROOT/'tools/fixtures/lighting_gpu_profile.inc').read_text()
            manifest['gpu_fixture_sha256']=hashlib.sha256(gpu.encode()).hexdigest()
            replace(backend, 'static GLenum primitive_mode(', gpu+'\nstatic GLenum primitive_mode(')
            text=backend.read_text()
            text,count=re.subn(r'(\bglDraw(?:ElementsBaseVertex|Elements|Arrays)\([^;]+;)',r'lighting_gpu_begin();\n\t\t\1\n\t\tlighting_gpu_end();',text)
            assert count==9, count
            backend.write_text(text)
            replace(backend, '\tdevice.frame++;', '\tlighting_gpu_frame(device.frame);\n\tdevice.frame++;')
            falloff=ROOT/paths[4]
            falloff.write_text('extern int lighting_profile_world;\n'+falloff.read_text())
            replace(falloff, '\tplasma_falloff_bound = TRUE;', '\tlighting_profile_world = stage == 1 ? 3 : 2;\n\tplasma_falloff_bound = TRUE;')
            replace(falloff, '\tplasma_specular_tint = 0x00FFFFFFUL;\n\tif (!plasma_falloff_bound)', '\tlighting_profile_world = 0;\n\tplasma_specular_tint = 0x00FFFFFFUL;\n\tif (!plasma_falloff_bound)')
        subprocess.run([sys.executable, 'configure.py', *saved], cwd=ROOT, check=True)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)
        shutil.copy2(ROOT/'build/windows/halo.exe', output/'halo-performance.exe')
        manifest['executable_sha256'] = hashlib.sha256((output/'halo-performance.exe').read_bytes()).hexdigest()
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2))
    finally:
        for path, content in originals.items():
            (ROOT/path).write_bytes(content)
        fixture.unlink(missing_ok=True)
        subprocess.run([sys.executable, 'configure.py', *saved], cwd=ROOT, check=True)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
