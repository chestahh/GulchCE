"""Capture actual framebuffer stages without adding a production debug mode."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((ROOT / 'build').resolve()):
        parser.error('Choose a fresh directory under build/')
    render = ROOT / 'source/render/render.c'
    backend = ROOT / 'port/linux/src/modern_environment_gl.inl'
    fixture = ROOT / 'tools/fixtures/modern_shadow_capture.inc'
    originals = {p: p.read_bytes() for p in (render, backend, fixture)}
    try:
        text = render.read_text()
        text = 'long shadow_composition_tick;\nvoid shadow_composition_readback(int);\n' + text
        for marker, calls in {
            '\t\trender_object_shadows();': '\t\tshadow_composition_readback(0);\n\t\trender_object_shadows();\n\t\tshadow_composition_readback(1);',
            '\t\tlights_render_diffuse();': '\t\tlights_render_diffuse();\n\t\tshadow_composition_readback(2);',
            '\t\tlights_render_specular();': '\t\tshadow_composition_readback(3);\n\t\tlights_render_specular();\n\t\tshadow_composition_readback(4);',
        }.items():
            assert text.count(marker) == 1
            text = text.replace(marker, calls)
        render.write_text(text)
        text = fixture.read_text()
        marker = '\tif (capture_ticks == 65)\n'
        assert text.count(marker) == 1
        text = 'extern long shadow_composition_tick;\n' + text.replace(marker,
            '\tshadow_composition_tick = capture_ticks;\n' + marker)
        fixture.write_text(text)
        backend.write_text('#include <stdio.h>\n' + backend.read_text() + r'''
extern long shadow_composition_tick;
void shadow_composition_readback(int stage)
{
    static int captured;
    unsigned int target, format, copy_source;
    int size[2], viewport[4];
    float center[2], *pixels;
    char name[64];
    FILE *file;
    if (!getenv("SHADOW_COMPOSITION_DUMP") || shadow_composition_tick != 160 || (captured & (1 << stage))) return;
    captured |= 1 << stage;
    if (!xgpu_gl_scene_target(&target, size, &format, &copy_source, viewport, center)) return;
    pixels = malloc(size[0] * size[1] * 4 * sizeof(float));
    if (!pixels) return;
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glReadPixels(0, 0, size[0], size[1], GL_RGBA, GL_FLOAT, pixels);
    snprintf(name, sizeof(name), "composition-stage-%d.bin", stage);
    file = fopen(name, "wb");
    if (file) {
        fwrite(size, sizeof(size), 1, file);
        fwrite(pixels, sizeof(float), size[0] * size[1] * 4, file);
        fclose(file);
    }
    free(pixels);
    xgpu_gl_end_external_pass();
}
''')
        subprocess.run([sys.executable, 'tools/environment_shadow_build.py', '--output', str(output)], cwd=ROOT, check=True)
        manifest_path = output / 'build-manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['composition_capture'] = True
        manifest['composition_source_hashes'] = {
            p.relative_to(ROOT).as_posix(): hashlib.sha256(data).hexdigest()
            for p, data in originals.items()}
        manifest_path.write_text(json.dumps(manifest, indent=2))
    finally:
        for p, data in originals.items():
            p.write_bytes(data)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
