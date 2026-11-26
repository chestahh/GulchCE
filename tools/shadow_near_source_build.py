"""Build isolated near-source diagnostics, restoring all inputs even on failure.

HALO_SHADOW_NEAR_SOURCE=muzzle|grenade|stuck uses the existing player/Marine
scene. Grenades are real projectiles; only their initial launch is controlled.
Normal builds contain neither these fixtures nor the environment controls.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def replace(source, before, after):
    if source.count(before) != 1:
        raise ValueError('Near-source integration changed: ' + before[:100])
    return source.replace(before, after, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--defer-restored-build', action='store_true', help='Outer builder restores and rebuilds ordinary sources')
    parser.add_argument('--reference-geometry', action='store_true',
                        help='Record the emitter and impact host as receivers for the offline grenade reference')
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(ROOT / 'build') or out.exists():
        parser.error('Use a fresh output directory beneath build/')
    paths = [ROOT / 'tools/fixtures/modern_shadow_capture.inc',
             ROOT / 'tools/fixtures/plasma_lighting_capture.c',
             ROOT / 'port/linux/src/modern_shadows_gl.c',
             ROOT / 'source/effects/effects.c',
             ROOT / 'port/linux/game/plasma_lighting.c',
             ROOT / 'source/game/player_control.c']
    collector = ROOT/'port/linux/game/modern_shadows.c'
    if args.reference_geometry:
        paths.append(collector)
    originals = {p: p.read_bytes() for p in paths}
    out.mkdir(parents=True)
    try:
        if args.reference_geometry:
            # Collect both the emitting shell and the struck body as camera receivers.
            # The offline depth pass removes their exact datum IDs for this light.
            text = replace(collector.read_text(), 'excluded_caster, grenade.casing);',
                           'grenade.casing != NONE ? NONE : excluded_caster, NONE);')
            start = 'if (!model_caster(model, object_get_node_matrices(iterator.index)'
            text = replace(text, start, 'near_reference_owner = iterator.index;\n\t\t' + start)
            end = 'object->object.forced_shader_permutation_index, values, views)) return 0;'
            text = replace(text, end, end + '\n\t\tnear_reference_owner = NONE;')
            collector.write_text('int near_reference_owner = -1;\n' + text)
        source = paths[0].read_text()
        source = replace(source, 'static void modern_capture_tick(const char *scene)',
                         (ROOT / 'tools/fixtures/shadow_near_source_capture.inc').read_text() +
                         '\nstatic void modern_capture_tick(const char *scene)')
        source = replace(source, 'int external = strcmp(scene, "ghost-engine") && !jackal;',
                         'int external = strcmp(scene, "ghost-engine") && !jackal && !getenv("HALO_SHADOW_NEAR_SOURCE");')
        source = replace(source, 'if (jackal && !shadow_wall())',
                         'if ((jackal || getenv("HALO_SHADOW_NEAR_SOURCE")) && !shadow_wall())')
        source = replace(source, 'if (!shadow_ready || !(subject = object_try_and_get(shadow_subject))) return;',
                         'if (!shadow_ready || !(subject = object_try_and_get(shadow_subject))) return;\n\tnear_source_tick();')
        paths[0].write_text(source)
        # Keep video captures long enough to include the natural detonation.
        source = paths[1].read_text()
        source = replace(source, '(getenv("HALO_SHADOW_CAPTURE_COMPOSITION") ? 220 : 160)',
                         '((getenv("HALO_SHADOW_CAPTURE_COMPOSITION") || getenv("HALO_SHADOW_NEAR_SOURCE")) ? 220 : 160)')
        paths[1].write_text(source)
        # The reference must distinguish opaque alpha-tested meshes from real
        # holes. Missing texture readback otherwise drops most model casters.
        source = '#include <stdio.h>\n#include <stdlib.h>\n' + paths[2].read_text()
        marker = '\t\tif (!alpha || target != GL_TEXTURE_2D) { failed = 1; return 0; }'
        source = replace(source, marker, marker + '''
        { static GLuint seen[256]; static int used; int i;
          for(i=0;i<used;i++) if(seen[i]==alpha) break;
          if(i==used && used<256) {
            int w=description.width,h=description.height;
            unsigned char *pixels; char name[80]; FILE *f;
            glBindTexture(GL_TEXTURE_2D,alpha);
            if(w>0 && h>0 && w<=4096 && h<=4096 && (pixels=malloc(w*h*4))!=NULL) {
              glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
              snprintf(name,sizeof(name),"shadow-alpha-%u.bin",alpha); f=fopen(name,"wb");
              if(f) { fwrite(&w,4,1,f);fwrite(&h,4,1,f);fwrite(pixels,4,w*h,f);fclose(f); }
              free(pixels); seen[used++]=alpha;
            }
          }
        }
''')
        if args.reference_geometry:
            source = replace(source, 'draws[draw_count].mesh = mesh - 1;',
                             'near_reference_owners[draw_count] = near_reference_owner;\n\tdraws[draw_count].mesh = mesh - 1;')
            source = replace(source, 'static int mesh_count,', 'extern int near_reference_owner;\n'
                             'static int near_reference_owners[SHADOW_DRAWS];\nstatic int mesh_count,')
            marker = '\tmap->view_map = 0;'
            source = replace(source, marker, r"""
    {
        extern long near_capture_tick(void);
        static long recorded[MODERN_SHADOW_LIGHTS];
        long tick = near_capture_tick();
        int slot = (int)(map-maps);
        if ((tick == 115 || tick == 130) && recorded[slot] != tick) {
            char name[80]; FILE *f;
            snprintf(name,sizeof(name),"shadow-owners-%ld-%d.bin",tick,slot);
            f=fopen(name,"wb");
            if (f) { fwrite(near_reference_owners,sizeof(int),draw_count,f); fclose(f); }
            recorded[slot]=tick;
        }
    }
""" + marker)
        paths[2].write_text(source)
        # Observe exact producer ownership before particles discard it. Keep
        # this inside the temporary diagnostic build, not the stock engine.
        source = paths[3].read_text()
        marker = '\t\t\t\tif (effect_allowed_by_environment(\n'
        source = replace(source, marker, '''
                { extern void platform_log(const char *, ...);
                  static int samples;
                  if (samples < 200 && strstr(tag_get_name(effect->definition_index), "plasma grenade")) {
                    samples++;
                    platform_log("NEAR emission owner %ld effect %s particle %s point %.6f %.6f %.6f direction %.6f %.6f %.6f velocity %.6f %.6f %.6f distribution %.6f radius %.6f %.6f",
                      effect->owner_object_index, tag_get_name(effect->definition_index), tag_get_name(particles->particle.index),
                      world_point.x, world_point.y, world_point.z, world_direction.i, world_direction.j, world_direction.k,
                      world_velocity.i, world_velocity.j, world_velocity.k, emission_radius,
                      particles->radius_lower_bound, particles->radius_upper_bound);
                  } }
''' + marker)
        source += '\n' + (ROOT/'tools/fixtures/shadow_emission_center.inc').read_text()
        paths[3].write_text(source)
        source = paths[4].read_text()
        marker = '\t\t\tfloat offset = plasma_lighting_attachments[index].forward_offset * scale;'
        source = replace(source, marker, marker + '''
            if (modern_shadows_enabled() && getenv("HALO_SHADOW_EMISSION_CENTER")) {
                extern int near_emission_origin(long, real_point3d *);
                extern void platform_log(const char *, ...);
                static int reports[2];
                struct object_datum *object = object_try_and_get(object_index);
                real_point3d original = *position;
                int attached = object && object->object.parent_object_index != NONE;
                if (near_emission_origin(object_index, position) && reports[attached] < 8) {
                    reports[attached]++;
                    platform_log("NEAR center matched owner %ld attached %d original %.9f %.9f %.9f candidate %.9f %.9f %.9f",
                        object_index, attached, original.x, original.y, original.z, position->x, position->y, position->z);
                }
            }
''')
        paths[4].write_text('#include <stdlib.h>\n' + source)
        source = paths[5].read_text()
        marker = '\tplayer_control_port_debug_throttle[local_player_index] = input.throttle;'
        paths[5].write_text(replace(source, marker,
            '\t{ extern float near_capture_strafe(void); float strafe = near_capture_strafe();\n'
            '\t  if (local_player_index == 0 && strafe != 0) input.throttle.j = strafe; }\n' + marker))
        subprocess.run([sys.executable, 'tools/plasma_visual_build.py', '--attachment-diagnostics', '--near-source-ticks',
                        '--shadow-geometry', '--defer-restored-build', '--output-dir', str(out)], cwd=ROOT, check=True)
        (out / 'near-source-manifest.json').write_text(json.dumps({
            'original_sha256': {p.relative_to(ROOT).as_posix(): hashlib.sha256(data).hexdigest()
                                for p, data in originals.items()},
            'reference_geometry': args.reference_geometry,
            'fixture_sha256': hashlib.sha256((ROOT / 'tools/fixtures/shadow_near_source_capture.inc').read_bytes()).hexdigest(),
            'emission_center_sha256': hashlib.sha256((ROOT / 'tools/fixtures/shadow_emission_center.inc').read_bytes()).hexdigest(),
            'builder_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'scope': 'Isolated diagnostic; no production source changes'}, indent=2))
    finally:
        for path, data in originals.items():
            path.write_bytes(data)
        for path, data in originals.items():
            assert path.read_bytes() == data
        if not args.defer_restored_build:
            subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
