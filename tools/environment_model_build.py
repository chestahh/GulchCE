"""Build M4 model-shadow controls; restore production source and ordinary build."""
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
    source = ROOT / 'port/linux/src/modern_environment_gl.inl'
    original = source.read_bytes()
    backend = ROOT / 'port/linux/src/modern_shadows_gl.c'
    backend_original = backend.read_bytes()
    try:
        text = source.read_text()
        marker = '    environment_model.enabled = environment_model.valid = 0;'
        assert text.count(marker) == 1
        text = text.replace(marker, marker + '\n    if (getenv("SHADOW_ENV_MODEL_DISABLE")) return 0;')
        marker = '    environment_model.enabled = environment_model.valid = success ? mask : 0;'
        assert text.count(marker) == 1
        text = text.replace(marker, marker + r'''
    { static int sample; sample++;
      if (!(sample % 60)) platform_log("ENV MODEL mask %d draws %d radius %.4f size %d", environment_model.enabled, draw_count, environment_model.radius, environment_model.size);
      if (!(sample % 120) && sample < 6000 && success) {
        char name[80]; FILE *f; int d;
        snprintf(name,sizeof(name),"environment-draws-%d.bin",sample); f=fopen(name,"wb");
        if(f) { fwrite(environment_model.frames,4,32,f); fwrite(&environment_model.radius,4,1,f); fwrite(&draw_count,4,1,f);
          for(d=0;d<draw_count;d++) { fwrite(&draws[d].mesh,4,3,f); fwrite(draws[d].matrices,sizeof(draws[d].matrices),1,f);
            fwrite(draws[d].uv_transform,sizeof(draws[d].uv_transform),1,f); fwrite(&draws[d].alpha,4,1,f); } fclose(f); }
      }
    }''')
        source.write_text('#include <stdio.h>\n' + text)
        text = backend.read_text()
        marker = 'glUniform1i(uniforms->environment_mask, environment_model.enabled);'
        assert text.count(marker) == 1
        text = text.replace(marker, 'glUniform1i(uniforms->environment_mask, getenv("SHADOW_ENV_MODEL_UNOCCLUDED") ? 0 : environment_model.enabled);')
        backend.write_text(text)
        subprocess.run([sys.executable, 'tools/plasma_visual_build.py', '--shadow-geometry', '--output-dir', str(output)], cwd=ROOT, check=True)
        path = output / 'build-manifest.json'
        manifest = json.loads(path.read_text())
        manifest['environment_model_probe'] = True
        manifest['environment_model_source_sha256'] = hashlib.sha256(original).hexdigest()
        manifest['environment_backend_source_sha256'] = hashlib.sha256(backend_original).hexdigest()
        path.write_text(json.dumps(manifest, indent=2))
    finally:
        source.write_bytes(original)
        backend.write_bytes(backend_original)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)

if __name__ == '__main__':
    main()
