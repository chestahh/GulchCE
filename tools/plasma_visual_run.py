"""Run the capture-only build with isolated data, retaining images and timing.

Example: python tools/plasma_visual_run.py --data-root C:/Games/GulchCE --fps 60 --enabled 1
Only this runner's copied maps and generated map caches are removed.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import statistics

parser = argparse.ArgumentParser()
parser.add_argument('--data-root', required=True, type=Path)
parser.add_argument('--fps', required=True, type=int, choices=[30, 60, 120])
parser.add_argument('--enabled', required=True, type=int, choices=[0, 1])
parser.add_argument('--label', default='')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
assert re.fullmatch(r'[a-zA-Z0-9_-]*', args.label)
out = root / f'build/plasma-visual/{args.fps}-{args.enabled}{"-" + args.label if args.label else ""}'
out.mkdir(parents=True, exist_ok=False)
(out / 'maps').mkdir()
(out / 'screenshots').mkdir()
for name in ['ui.map', 'bloodgulch.map']:
    shutil.copy2(args.data_root / 'maps' / name, out / 'maps' / name)
(out / 'init.txt').write_text('map_name levels\\test\\bloodgulch\\bloodgulch\n')
env = {k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
env['PATH'] = str(root / 'build/windows') + os.pathsep + env.get('PATH', '')
env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='1',
           HALO_NO_AUDIO='1', HALO_MAX_FPS=str(args.fps), HALO_NO_VSYNC='1', HALO_EXIT_AFTER='13',
           HALO_NET_ONLINE='0', HALO_NET_PUBLIC_LOBBY='0', HALO_UPDATE_ANSWER='never',
           HALO_CRASH_REPORTS='no', HALO_WINDOW_SIZE='640x480', HALO_PLASMA_LIGHTING=str(args.enabled),
           HALO_SCREENSHOT_DIR=str(out/'screenshots'))
with (out/'stdout.log').open('wb') as so, (out/'stderr.log').open('wb') as se:
    process = subprocess.Popen([str(root/'build/plasma-visual/halo-capture.exe')], cwd=out,
                               env=env, stdout=so, stderr=se, creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        process.wait(timeout=45)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
logs = '\n'.join(p.read_text(errors='replace') for p in out.glob('*.log'))
times = [int(x) for x in re.findall(r'PLASMA frame (\d+)', logs)]
deltas = [b-a for a,b in zip(times, times[1:]) if b>a]
report = dict(cap=args.fps, enabled=args.enabled, exit=process.returncode,
              screenshots=len(list((out/'screenshots').glob('*.bmp'))), frames=len(times),
              selected_frames=len(re.findall(r'selected [1-9]', logs)),
              average_fps=1000*(len(times)-1)/(times[-1]-times[0]) if len(times)>1 else None,
              median_ms=statistics.median(deltas) if deltas else None,
              shots=len(re.findall('PLASMA shot', logs)))
(out/'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
for path in out.rglob('*.map'):
    assert path.resolve().is_relative_to(out.resolve())
    path.unlink()
if process.returncode or report['shots'] != 3 or not report['screenshots']:
    raise SystemExit('Capture incomplete; inspect report.json and logs')
if bool(report['selected_frames']) != bool(args.enabled):
    raise SystemExit('Lighting selection did not match the requested mode')
if report['average_fps'] is None or report['average_fps'] < args.fps * .95:
    raise SystemExit('Capture did not sustain the requested FPS within 5 percent')
