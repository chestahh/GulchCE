"""Build opt-in shadow diagnostics, restoring every production input afterwards.

Controls exist only in this capture binary. No setting or renderer is changed
in normal builds. The muzzle control invokes the real light event at a fixed
tick, without spawning bullets; it is a placement test, not a firing test.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import argparse

ROOT = Path(__file__).resolve().parents[1]
PATHS = ['source/render/render_objects.c',
         'port/linux/game/ambient_emitter_lighting.c',
         'tools/fixtures/modern_shadow_capture.inc',
         'tools/plasma_visual_build.py',
         'port/linux/src/modern_shadows_gl.c', 'port/linux/game/muzzle_lighting.c']


def replace(text, before, after):
    if text.count(before) != 1:
        raise ValueError('Capture integration changed: ' + before[:100])
    return text.replace(before, after, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/shadow-comparison/capture')
    output = parser.parse_args().output.resolve()
    if not output.is_relative_to((ROOT/'build').resolve()) or (output/'halo-capture.exe').exists():
        parser.error('Use a fresh output directory beneath build/')
    original = {ROOT/p: (ROOT/p).read_bytes() for p in PATHS}
    output.mkdir(parents=True, exist_ok=True)
    try:
        p = ROOT/PATHS[0]
        text = p.read_text()
        text = replace(text, '\tobject_get_bounding_sphere(data->object_index, &center, &radius);',
            '\tif (getenv("SHADOW_COMPARE_NO_STOCK")) return FALSE;\n'
            '\tobject_get_bounding_sphere(data->object_index, &center, &radius);')
        p.write_text(text)
        p = ROOT/PATHS[5]
        text = p.read_text()
        text = replace(text, '\tif (third_person_active(render.local_player_index) ||\n\t\t!first_person_weapon_adjust_light(pulse->weapon, name, position, &forward, &up))',
            '\tif (getenv("SHADOW_COMPARE_WORLD_MARKER") || third_person_active(render.local_player_index) ||\n\t\t!first_person_weapon_adjust_light(pulse->weapon, name, position, &forward, &up))')
        marker = '\tif (profile->forward_offset != 0.0f)'
        text = replace(text, marker, '''
    { static int seen; struct object_marker actual;
      if(seen++<16 && object_get_marker_by_name(pulse->weapon,name,&actual,1))
        platform_log("COMPARE MUZZLE chosen %.6f %.6f %.6f world %.6f %.6f %.6f distance %.6f",
          position->x,position->y,position->z,actual.matrix.position.x,actual.matrix.position.y,actual.matrix.position.z,
          distance3d(position,&actual.matrix.position));
    }
''' + marker)
        p.write_text(text)
        p = ROOT/PATHS[4]
        text = '#include <stdio.h>\n#include <stdlib.h>\n' + p.read_text()
        marker = '\t\tif (!alpha || target != GL_TEXTURE_2D) { failed = 1; return 0; }'
        text = replace(text, marker + '\n', marker + '''
        { static GLuint seen[256]; static int used; int i;
          for(i=0;i<used;i++) if(seen[i]==alpha) break;
          if(i==used && used<256) {
            GLint w=0,h=0; unsigned char *pixels; char name[80]; FILE *f;
            glBindTexture(GL_TEXTURE_2D,alpha);
            w=description.width; h=description.height;
            if(w>0 && h>0 && w<=4096 && h<=4096 && (pixels=malloc(w*h*4))!=NULL) {
              glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
              snprintf(name,sizeof(name),"shadow-alpha-%u.bin",alpha); f=fopen(name,"wb");
              if(f) { fwrite(&w,4,1,f);fwrite(&h,4,1,f);fwrite(pixels,4,w*h,f);fclose(f); }
              free(pixels); seen[used++]=alpha;
            }
          }
        }
''')
        p.write_text(text)
        p = ROOT/PATHS[1]
        text = p.read_text()
        text = replace(text, '\t\t\tmarker = component ? profile->second_marker : 0;',
            '\t\t\tif (getenv("SHADOW_COMPARE_ENGINE") && component != atoi(getenv("SHADOW_COMPARE_ENGINE"))) return FALSE;\n'
            '\t\t\tmarker = component ? profile->second_marker : 0;')
        p.write_text(text)
        p = ROOT/PATHS[2]
        text = '#include "muzzle_lighting.h"\n' + p.read_text()
        text = replace(text, '\tint external = strcmp(scene, "ghost-engine") && !jackal;',
            '\tint external = strcmp(scene, "ghost-engine") && !jackal && !getenv("SHADOW_COMPARE_MUZZLE");')
        text = replace(text, 'if (jackal && !shadow_wall())',
            'if ((jackal || (local && getenv("SHADOW_COMPARE_WALL"))) && !shadow_wall())')
        text = replace(text, '\t\tshadow_camera = capture_camera;',
            '\t\tif (!strcmp(scene,"ghost-engine") && getenv("SHADOW_COMPARE_GHOST_PAIR")) {\n'
            '\t\t\treal_point3d other = shadow_anchor; other.x += shadow_right.i * 1.2f; other.y += shadow_right.j * 1.2f;\n'
            '\t\t\tshadow_create(\'vehi\', "vehicles\\\\ghost\\\\ghost_mp", &other, &shadow_forward);\n\t\t}\n'
            '\t\tshadow_camera = capture_camera;')
        text = replace(text, '\tif (capture_ticks == 80 && !strcmp(scene, "corpse"))',
            '\tif (getenv("SHADOW_COMPARE_MUZZLE") && shadow_weapon != NONE && capture_ticks >= 95)\n'
            '\t\tmuzzle_lighting_shot(shadow_weapon, 0);\n'
            '\tif (capture_ticks == 80 && !strcmp(scene, "corpse"))')
        p.write_text(text)
        # A third export samples the settled scene. Cross-cap comparisons must
        # use tick-labelled gameplay captures, not these render-frame indices.
        p = ROOT/PATHS[3]
        text = p.read_text()
        text = replace(text, 'frame==120 || frame==180',
            '(frame==120 || frame==180 || frame==300)')
        p.write_text(text)
        subprocess.run([sys.executable, 'tools/plasma_visual_build.py',
                        '--shadow-geometry', '--output-dir', str(output)], cwd=ROOT, check=True)
        (output/'comparison-inputs.json').write_text(json.dumps({
            'original_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(b).hexdigest() for p,b in original.items()},
            'capture_builder_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'controls': ['SHADOW_COMPARE_NO_STOCK', 'SHADOW_COMPARE_ENGINE',
                         'SHADOW_COMPARE_GHOST_PAIR', 'SHADOW_COMPARE_WALL', 'SHADOW_COMPARE_MUZZLE',
                         'SHADOW_COMPARE_WORLD_MARKER'],
            'scope': 'Capture-only diagnostic; not a replacement renderer'}, indent=2))
    finally:
        for p, content in original.items():
            p.write_bytes(content)
        for p, content in original.items():
            assert p.read_bytes() == content
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
