"""Capture matched shadow acceptance keyframes using the real game renderer.

Requires a plasma_visual_build.py capture build and user-owned Xbox maps.
Only disposable BMPs belonging to these runs are removed after lossless PNG
verification. Logs, manifests, geometry diagnostics and saves remain available.
These short instrumented captures are not combat performance measurements.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCENES = {
    'warthog': 'sidewinder', 'ghost': 'sidewinder', 'ghost-engine': 'sidewinder',
    'jackal-wall': 'c40', 'player': 'sidewinder', 'marine': 'b40',
    'corpse': 'sidewinder', 'first-person': 'sidewinder',
    'dropped-weapon': 'sidewinder', 'frag-grenade': 'sidewinder', 'plasma-grenade': 'sidewinder',
}
TICKS = [100, 130, 160, 190, 220]


def run(args):
    output = ROOT/'build/modern-refinement'/args.label
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for scene in args.scenes:
        for mode in args.modes:
            label = f'{args.label}-{scene}-{mode}'
            capture = ROOT/f'build/plasma-visual/{args.fps}-1-{label}'
            command = [sys.executable, str(ROOT/'tools/plasma_visual_run.py'),
                       '--data-root', str(args.data_root.resolve()), '--capture-dir', str(args.capture_dir.resolve()),
                       '--fps', str(args.fps), '--enabled', '1', '--modern-shadows', mode,
                       '--shadow-resolution', str(args.shadow_resolution),
                       '--occlusion', '1', '--map', SCENES[scene], '--shadow-scene', scene,
                       '--resolution', args.resolution, '--startup-grace', '5', '--label', label]
            if args.per_pixel:
                command.append('--per-pixel')
            with (output/f'{scene}-{mode}.log').open('w') as log:
                result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            entry = dict(scene=scene, mode=mode, returncode=result.returncode,
                         path=capture.relative_to(ROOT).as_posix(), command=command)
            if capture.is_dir():
                (capture/'run-command.json').write_text(json.dumps(command, indent=2))
                logs = (capture/'stderr.log').read_text(errors='replace')
                entry['samples'] = re.findall(r'SHADOW SCENE (?:sample|light|wall|created)[^\r\n]*', logs)
                frames = sorted((capture/'screenshots').glob('*.bmp'))
                entry['images'] = []
                for index, frame in enumerate(frames):
                    target = frame.with_suffix('.png')
                    with Image.open(frame) as image:
                        pixels = image.convert('RGB').tobytes()
                        image.save(target)
                    with Image.open(target) as image:
                        if image.convert('RGB').tobytes() != pixels:
                            raise RuntimeError(f'PNG verification failed: {target}')
                    if not frame.resolve().is_relative_to(capture.resolve()):
                        raise RuntimeError('Capture path escaped the isolated run')
                    frame.unlink()
                    entry['images'].append(dict(tick=TICKS[index] if len(frames) == 5 else None,
                                                path=target.relative_to(ROOT).as_posix(),
                                                sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
                if len(frames) != 5:
                    entry['error'] = 'Expected exactly five baseline keyframes'
            results.append(entry)
            (output/'index.json').write_text(json.dumps(results, indent=2))
            print(f'{scene} {mode}: exit {result.returncode}, {len(entry.get("images", []))} keyframes', flush=True)
    if any(row['returncode'] or row.get('error') for row in results):
        raise SystemExit('Some baseline scenes failed; see index.json and retained logs')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root', required=True, type=Path)
    parser.add_argument('--capture-dir', required=True, type=Path)
    parser.add_argument('--label', required=True)
    parser.add_argument('--scenes', nargs='+', choices=SCENES, default=list(SCENES))
    parser.add_argument('--modes', nargs='+', choices=['off', 'hard', 'soft'], default=['off', 'hard', 'soft'])
    parser.add_argument('--fps', type=int, choices=[30, 60, 120], default=60)
    parser.add_argument('--resolution', default='2560x1440')
    parser.add_argument('--shadow-resolution', type=int, choices=[128, 256, 512, 1024, 2048, 4096], default=1024,
                        help='Acceptance setting; the game default remains 128')
    parser.add_argument('--per-pixel', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch('[a-zA-Z0-9_-]+', args.label):
        parser.error('--label must use letters, digits, hyphens or underscores')
    run(args)
