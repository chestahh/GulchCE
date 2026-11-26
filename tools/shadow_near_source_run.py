"""Run plasma_visual_run with an explicit near-source diagnostic manifest.

SHADOW_NEAR_SOURCE must be muzzle, grenade or stuck. Other inherited HALO
controls remain filtered by the ordinary isolated runner.
"""
import json
import os
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    kind = os.environ.get('SHADOW_NEAR_SOURCE')
    if kind not in ('muzzle', 'grenade', 'stuck') or '--capture-dir' not in sys.argv:
        raise SystemExit('Specify SHADOW_NEAR_SOURCE and --capture-dir')
    build = Path(sys.argv[sys.argv.index('--capture-dir') + 1])
    if not (build / 'near-source-manifest.json').is_file():
        raise SystemExit('A near-source capture build is required')
    if os.environ.get('SHADOW_EMISSION_CENTER') == '1' and not json.loads(
            (build/'near-source-manifest.json').read_text()).get('emission_center_sha256'):
        raise SystemExit('This capture build does not contain the emitter-centre candidate')
    path = ROOT / 'tools/plasma_visual_run.py'
    source = path.read_text()
    marker = "env['PATH'] = str(root / 'build/windows') + os.pathsep + env.get('PATH', '')"
    if source.count(marker) != 1:
        raise ValueError('Capture runner changed')
    source = source.replace(marker, "env['HALO_SHADOW_NEAR_SOURCE'] = os.environ['SHADOW_NEAR_SOURCE']\n"
        "if os.environ.get('SHADOW_EMISSION_CENTER') == '1':\n    env['HALO_SHADOW_EMISSION_CENTER'] = '1'\n"
        "env['HALO_MUZZLE_LIGHTING'] = str(int(os.environ['SHADOW_NEAR_SOURCE'] == 'muzzle' and args.enabled))\n" + marker)
    namespace = dict(__name__='__main__', __file__=str(path))
    capture_failure = None
    try:
        exec(compile(source, str(path), 'exec'), namespace)
    except SystemExit as error:
        capture_failure = error
    finally:
        if 'out' in namespace:
            (namespace['out'] / 'near-source-controls.json').write_text(json.dumps(
                dict(kind=kind, emission_center=os.environ.get('SHADOW_EMISSION_CENTER') == '1'), indent=2))
    if 'out' not in namespace:
        if capture_failure is not None:
            raise capture_failure
        raise RuntimeError('Capture runner did not establish an output directory')
    out = namespace['out']
    logs = (out / 'stderr.log').read_text(errors='replace')
    if os.environ.get('SHADOW_EMISSION_CENTER') == '1' and not re.search(
            r'NEAR center matched owner -?\d+ attached ' + ('1' if kind == 'stuck' else '[01]'), logs):
        raise SystemExit('Emitter-centre candidate did not match the requested lifecycle state')
    if kind != 'muzzle' and ('NEAR created ' + kind not in logs or 'NEAR removed' not in logs):
        raise SystemExit('Grenade lifecycle incomplete; inspect logs')
    if kind == 'stuck' and not any(int(parent) != -1 for parent in
            re.findall(r'NEAR grenade tick \d+ id -?\d+ parent (-?\d+)', logs)):
        raise SystemExit('Grenade never attached; inspect logs')
    if capture_failure is not None:
        raise capture_failure


if __name__ == '__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
