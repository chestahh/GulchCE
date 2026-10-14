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
parser.add_argument('--map', default='bloodgulch', choices=['bloodgulch', 'carousel', 'a10', 'ce-deathisland'])
parser.add_argument('--kind', choices=['rifle','pistol','charged','rocket'], default='rifle')
parser.add_argument('--mixed', action='store_true')
parser.add_argument('--model', action='store_true')
parser.add_argument('--occlusion', type=int, choices=[0,1], default=0)
parser.add_argument('--probe', action='store_true')
parser.add_argument('--combat', action='store_true')
parser.add_argument('--door', action='store_true')
parser.add_argument('--toggle', action='store_true')
parser.add_argument('--span', type=float)
parser.add_argument('--burst', type=int, choices=range(1,33), default=1)
parser.add_argument('--specular-only', action='store_true')
parser.add_argument('--per-pixel', action='store_true')
parser.add_argument('--distance', type=float)
parser.add_argument('--height', type=float)
parser.add_argument('--drop', type=float)
parser.add_argument('--receiver-distance', type=float)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
assert re.fullmatch(r'[a-zA-Z0-9_-]*', args.label)
out = root / f'build/plasma-visual/{args.fps}-{args.enabled}{"-" + args.label if args.label else ""}'
out.mkdir(parents=True, exist_ok=False)
(out / 'maps').mkdir()
(out / 'screenshots').mkdir()
manifest = root/'build/plasma-visual/build-manifest.json'
if manifest.exists():
    shutil.copy2(manifest, out/'build-manifest.json')
shutil.copy2(args.data_root/'maps/ui.map',out/'maps/ui.map')
if args.map=='ce-deathisland':
    (out/'custom_maps').mkdir()
    for name in ['deathisland_451254bb.map','bitmaps.map','sounds.map','loc.map']:
        shutil.copy2(args.data_root/'custom_maps'/name,out/'custom_maps'/name)
    level='custom_maps\\deathisland_451254bb'
else:
    shutil.copy2(args.data_root/'maps'/(args.map+'.map'),out/'maps'/(args.map+'.map'))
    level=f'levels\\{args.map}\\{args.map}' if args.map=='a10' else f'levels\\test\\{args.map}\\{args.map}'
(out/'init.txt').write_text(f'map_name {level}\n')
if args.specular_only:
    with (out/'init.txt').open('a') as init:
        init.write('(set rasterizer_environment_diffuse_lights false)\n')

env = {k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
env['PATH'] = str(root / 'build/windows') + os.pathsep + env.get('PATH', '')
env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='1',
           HALO_NO_AUDIO='1', HALO_MAX_FPS=str(args.fps), HALO_NO_VSYNC='1', HALO_EXIT_AFTER='25' if args.door else '13',
           HALO_NET_ONLINE='0', HALO_NET_PUBLIC_LOBBY='0', HALO_UPDATE_ANSWER='never',
           HALO_CRASH_REPORTS='no', HALO_WINDOW_SIZE='640x480', HALO_PLASMA_LIGHTING=str(args.enabled),
           HALO_SCREENSHOT_DIR=str(out/'screenshots'), HALO_PLASMA_OCCLUSION=str(args.occlusion))
for key in ['distance', 'height', 'receiver_distance', 'span', 'burst', 'drop']:
    value = getattr(args, key)
    if value is not None:
        env['HALO_PLASMA_CAPTURE_'+key.upper()] = str(value)
env['HALO_PLASMA_CAPTURE_KIND'] = args.kind
for key in ['combat','door','toggle','mixed']:
    if getattr(args,key):env['HALO_PLASMA_CAPTURE_'+key.upper()]='1'
if args.probe:
    env['HALO_PLASMA_CAPTURE_PROBE']='1'
if args.model:
    env['HALO_PLASMA_CAPTURE_MODEL'] = '1'
if args.per_pixel:
    env['HALO_PER_PIXEL_LIGHTING'] = '1'
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
report = dict(kind=args.kind, mixed=args.mixed, cap=args.fps, enabled=args.enabled, exit=process.returncode,
              screenshots=len(list((out/'screenshots').glob('*.bmp'))), frames=len(times),
              selected_frames=len(re.findall(r'selected [1-9]', logs)),
              average_fps=1000*(len(times)-1)/(times[-1]-times[0]) if len(times)>1 else None,
              median_ms=statistics.median(deltas) if deltas else None,
              shots=len(re.findall('PLASMA shot', logs)),
              model_draws=sum(int(x) for x in re.findall(r'PLASMA draws model (\d+)', logs)),
              specular_draws=sum(int(x) for x in re.findall(r'PLASMA draws model \d+ specular (\d+)', logs)),
              max_selected=max([int(x) for x in re.findall(r'selected (\d+)',logs)] or [0]), burst=args.burst, combat=args.combat, door=args.door, toggle=args.toggle, occlusion=args.occlusion, probe=args.probe, occlusion_updates=len(re.findall('PLASMA occlusion extent',logs)), map=args.map, model=args.model, per_pixel=args.per_pixel, specular_only=args.specular_only)
report['authored_light_flags'] = sorted({int(x, 16) for x in re.findall(r'PLASMA enabled [01] light flags ([0-9a-f]+)', logs)})
report['homing_shots'] = len(re.findall('PLASMA homing', logs))
report['selected_tags'] = sorted(set(re.findall(r'PLASMA prepare [^\r\n]+ tag ([^\r\n]+)', logs)))
(out/'report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report), flush=True)
for path in out.rglob('*.map'):
    assert path.resolve().is_relative_to(out.resolve())
    path.unlink()
if process.returncode or (report['shots'] != 3 and not args.combat) or not report['screenshots']:
    raise SystemExit('Capture incomplete; inspect report.json and logs')
if bool(report['selected_frames']) != bool(args.enabled):
    raise SystemExit('Lighting selection did not match the requested mode')
if report['average_fps'] is None or report['average_fps'] < args.fps * .95:
    raise SystemExit('Capture did not sustain the requested FPS within 5 percent')

if args.enabled and args.model and not report['model_draws']:
    raise SystemExit('Capture never reached a selected model draw')
if args.enabled and args.specular_only:
    flags = report['authored_light_flags']
    # Bit 1 is _light_definition_no_specular_bit in object_lights.c.
    if flags and all(value & 2 for value in flags):
        if report['specular_draws']:
            raise SystemExit('Capture ignored the authored no-specular flag')
    elif not report['specular_draws']:
        raise SystemExit('Capture never reached a selected specular draw')

if args.enabled and args.occlusion and not report['occlusion_updates']:
    raise SystemExit('Capture did not update any occlusion fields')

if args.combat and 'PLASMA combat equipped' not in logs:
    raise SystemExit('Combat fixture did not equip the requested weapon')
if args.probe and args.enabled and report['max_selected'] < args.burst:
    raise SystemExit('Stress capture did not select the requested simultaneous light count')
if args.door:
    positions = [float(x) for x in re.findall(r'PLASMA door position ([0-9.]+)', logs)]
    if not positions or min(positions) > .1 or max(positions) < .9:
        raise SystemExit('Door did not complete both open and closed states')
if args.toggle:
    intervals = re.split(r'PLASMA toggle ([01])', logs)
    if len(intervals) < 5:
        raise SystemExit('Toggle capture did not exercise both states')
    for i in range(1, len(intervals)-1, 2):
        if intervals[i] == '0' and 'PLASMA occlusion extent' in intervals[i+1]:
            raise SystemExit('Occlusion updated while disabled')

expected_tags = {
    'rifle': r'weapons\plasma rifle\bolt',
    'pistol': r'weapons\plasma pistol\bolt',
    'charged': r'weapons\plasma rifle\charged bolt',
    'rocket': r'weapons\rocket launcher\rocket',
}
if args.enabled:
    expected = {expected_tags[args.kind]}
    if args.mixed:
        expected = {value for kind, value in expected_tags.items() if args.kind == 'rocket' or kind != 'rocket'}
    if not expected.issubset(report['selected_tags']):
        raise SystemExit('Capture did not select every requested projectile kind')
if not args.enabled and report['occlusion_updates']:
    raise SystemExit('Disabled plasma lighting still updated occlusion')
if args.kind == 'charged' and args.model and not args.probe and not args.combat and report['homing_shots'] != 3:
    raise SystemExit('Charged-shot capture did not exercise the homing target')
