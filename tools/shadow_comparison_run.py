"""Run the existing isolated capture runner with comparison-only controls.

Accepts plasma_visual_run.py's arguments. SHADOW_COMPARE_* variables are
recorded in the resulting report. Requires shadow_comparison_build.py output.
"""
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    if '--capture-dir' not in sys.argv:
        raise SystemExit('An explicit comparison capture build is required')
    build = Path(sys.argv[sys.argv.index('--capture-dir')+1])
    if not (build/'comparison-inputs.json').is_file():
        raise SystemExit('Not a comparison capture build')
    path = ROOT/'tools/plasma_visual_run.py'
    source = path.read_text()
    marker = "env['PATH'] = str(root / 'build/windows') + os.pathsep + env.get('PATH', '')"
    assert source.count(marker) == 1
    source = source.replace(marker,
        "if os.environ.get('SHADOW_COMPARE_MUZZLE'):\n    env['HALO_MUZZLE_LIGHTING'] = '1'\n" + marker)
    namespace = dict(__name__='__main__', __file__=str(path))
    try:
        exec(compile(source,str(path),'exec'), namespace)
    finally:
        if 'out' in namespace:
            out = namespace['out']
            (out/'comparison-controls.json').write_text(json.dumps({
                k:v for k,v in os.environ.items() if k.startswith('SHADOW_COMPARE_')},indent=2))


if __name__=='__main__':main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
