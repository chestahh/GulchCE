"""Build capture-only marker diagnostics; restore production sources on exit."""
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
    if not output.is_relative_to((ROOT/'build').resolve()) or output.exists():
        parser.error('Use a fresh output directory beneath build/')
    files = [ROOT/f'port/linux/game/{name}_lighting.c' for name in ('muzzle', 'ammo_counter', 'needler')]
    original = {path: path.read_bytes() for path in files}
    try:
        for path in files:
            text = path.read_text()
            helper = r'''static void source_pose_probe(long weapon, long world, const char *name, const real_point3d *chosen)
{
    static long last_tick = NONE;
    struct object_marker marker;
    real_point3d fp;
    real_vector3d forward, up;
    int valid;
    long tick = game_time_get();
    if (tick == last_tick || tick % 10) return;
    last_tick = tick;
    if (!object_get_marker_by_name(world, name, &marker, 1)) return;
    valid = first_person_weapon_adjust_light(weapon, name, &fp, &forward, &up);
    platform_log("SOURCE POSE MODULE tick %ld third %d fp %d world_error %.8f fp_error %.8f marker %s",
        tick, third_person_active(render.local_player_index), valid,
        distance3d(chosen, &marker.matrix.position), valid ? distance3d(chosen, &fp) : -1.0f, name);
}

'''.replace('MODULE', path.stem)
            anchor = 'static int muzzle_position' if path == files[0] else 'int '+path.stem+'_position'
            assert text.count(anchor) == 1
            text = text.replace(anchor, helper+anchor, 1)
            if path == files[0]:
                anchor = '\tif (profile->forward_offset != 0.0f)'
                probe = '\tsource_pose_probe(pulse->weapon, TEST_FLAG(weapon->object.flags, _object_invisible_bit) ? pulse->owner : pulse->weapon, name, position);\n'
            elif path == files[1]:
                anchor = '\t/* Broad rear/up-facing emission'
                probe = '\tsource_pose_probe(index, index, "primary trigger", position);\n'
            else:
                anchor = '\t*direction = up;'
                probe = '\tsource_pose_probe(index, index, "smoke", position);\n'
            assert text.count(anchor) == 1
            path.write_text(text.replace(anchor, probe+anchor, 1))
        subprocess.run([sys.executable, 'tools/plasma_visual_build.py', '--output-dir', str(output)], cwd=ROOT, check=True)
        manifest = json.loads((output/'build-manifest.json').read_text())
        manifest['source_pose_diagnostics'] = True
        for path, data in original.items():
            manifest['source_hashes'][path.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
        (output/'build-manifest.json').write_text(json.dumps(manifest, indent=2))
    finally:
        for path, data in original.items():
            path.write_bytes(data)
        subprocess.run(['ninja', 'windows'], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
