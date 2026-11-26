"""Run a sustained, isolated Windows bot battle and summarize frame pacing.

One instance per invocation. Use sequential invocations for comparable timings.
Only the dedicated performance executable contains passive measurement hooks.
"""
import argparse
import csv
import ctypes
from concurrent.futures import ThreadPoolExecutor
from ctypes import wintypes
import hashlib
from itertools import pairwise
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time
import tomllib
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]
LIGHTS = ['plasma_lighting', 'muzzle_lighting', 'jackal_shield_lighting',
          'covenant_shield_lighting', 'fuel_rod_lighting', 'ammo_counter_lighting',
          'needler_lighting', 'plasma_occlusion', 'shield_glow',
          'weapon_glow', 'vehicle_lighting', 'world_lighting']
QUALITY = ['anti_aliasing', 'anisotropic_filtering', 'shadow_resolution',
           'per_pixel_lighting', 'model_detail', 'hdr', 'hdr_output',
           'viewmodel_shield', 'shield_look', 'fov', 'viewmodel_fov']


def distribution(values, cap):
    ordered = sorted(values)
    if not ordered:
        raise ValueError('No frame samples')
    percentile = lambda p: ordered[max(0, math.ceil(len(ordered)*p)-1)]
    return dict(frames=len(values), average_fps=1000/statistics.mean(values),
                p50_ms=percentile(.5), p95_ms=percentile(.95), p99_ms=percentile(.99),
                worst_ms=ordered[-1], low_1pct_fps=1000/statistics.mean(ordered[-max(1, math.ceil(len(ordered)*.01)):]),
                budget_ms=1000/cap,
                over_budget_percent=100*sum(v>1000/cap for v in values)/len(values),
                over_1_5_budget_percent=100*sum(v>1500/cap for v in values)/len(values),
                over_16_7ms=sum(v>16.7 for v in values), over_33ms=sum(v>33 for v in values),
                over_50ms=sum(v>50 for v in values), over_100ms=sum(v>100 for v in values))


def sustained_pacing(measured, warmup, seconds, targets):
    # Count frame completions in fixed wall-clock windows, including empty
    # seconds during a stall. Frames on a boundary belong to the next second.
    fps = [0] * seconds
    for row in measured:
        fps[int(row['seconds'] - warmup)] += 1
    targets = sorted(set(targets))
    bands = {}
    for target in targets:
        total = episodes = longest = streak = 0
        for count in fps:
            if count < target:
                total += 1
                if streak == 0:
                    episodes += 1
                streak += 1
                longest = max(longest, streak)
            else:
                streak = 0
        bands[str(target)] = dict(seconds_below=total, episodes=episodes,
                                  longest_below_seconds=longest)
    result = dict(fps_per_second=fps, targets=bands)
    # Separate the frames between simulation ticks from those that advanced the
    # game clock. Both timings include rendering. A reset has no comparable tick.
    tick_frames = []
    for previous, row in pairwise(measured):
        before, after = previous.get('tick'), row.get('tick')
        if before is not None and after is not None and after >= before:
            window = int(row['seconds'] - warmup)
            tick_frames.append((fps[window], after > before, row['frame_ms']))
    if tick_frames:
        result['tick_frame_times'] = {}
        for target in targets:
            groups = {}
            for count, advanced, duration in tick_frames:
                band = 'below' if count < target else 'at_or_above'
                kind = 'tick_advanced' if advanced else 'same_tick'
                groups.setdefault(band, {}).setdefault(kind, []).append(duration)
            result['tick_frame_times'][str(target)] = {
                band: {kind: dict(frames=len(times), mean_ms=statistics.mean(times))
                       for kind, times in kinds.items()}
                for band, kinds in groups.items()
            }
    return result


def summarize(rows, warmup, seconds, cap, captures=True):
    measured = [r for r in rows if warmup <= r['seconds'] < warmup+seconds and r['frame_ms']>0]
    # Older diagnostic binaries still mark capture slots when readback is off.
    # Such frames must remain in pacing results; there was no capture overhead.
    clean = [r for r in measured if not captures or not r['capture_excluded']]
    result = dict(raw=distribution([r['frame_ms'] for r in measured], cap),
                  pacing=distribution([r['frame_ms'] for r in clean], cap),
                  excluded_capture_frames=len(measured)-len(clean),
                  measured_span=measured[-1]['seconds']-measured[0]['seconds'],
                  selected_max=max(r['selected'] for r in measured),
                  selected_mean=statistics.mean(r['selected'] for r in measured),
                  selected_frame_percent=100*sum(r['selected']>0 for r in measured)/len(measured),
                  scene_lights_max=max(r['scene_lights'] for r in measured),
                  occlusion_updates_max=max(r['occlusion_updates'] for r in measured),
                  occlusion_mean_ms=statistics.mean(r.get('occlusion_ms',0) for r in clean),
                  occlusion_max_ms=max(r.get('occlusion_ms',0) for r in clean),
                  minutes=[])
    # A capture stalls the game. Dropping that frame would still leave its
    # elapsed time in the window. Use complete, capture-free traces for this.
    if not captures and rows[0]['seconds'] <= warmup and rows[-1]['seconds'] >= warmup + seconds:
        result['sustained'] = sustained_pacing(measured, warmup, seconds, (cap, 240, 360))
    if all('shadowed' in r for r in measured):
        result['shadowed_frame_percent'] = 100*sum(r['shadowed']>0 for r in measured)/len(measured)
        result['shadowed_mean'] = statistics.mean(r['shadowed'] for r in measured)
        result['shadowed_max'] = max(r['shadowed'] for r in measured)
        result['multiple_shadow_frame_percent'] = 100*sum(r['shadowed']>1 for r in measured)/len(measured)
    if all('sun_casters' in r for r in measured):
        result['sun_frame_percent'] = 100*sum(r['sun_casters']>0 for r in measured)/len(measured)
        result['sun_families_max'] = max(r['sun_casters'] for r in measured)
        result['sun_and_eight_local_percent'] = 100*sum(r['sun_casters']>0 and r['shadowed']==8 for r in measured)/len(measured)
    if all('spot_mask' in r for r in measured):
        masks = [int(r['spot_mask']) for r in measured]
        result['peak_local_shadows_including_beam'] = int(max(r['shadowed'] + bool(m & 4) for r,m in zip(measured,masks)))
        result['both_fixture_percent'] = 100*sum((v & 3) == 3 for v in masks)/len(masks)
        result['flashlight_percent'] = 100*sum(bool(v & 4) for v in masks)/len(masks)
        result['flashlight_and_seven_local_percent'] = 100*sum(bool(v & 4) and r['shadowed']==7 for r,v in zip(measured,masks))/len(masks)
        result['flashlight_and_eight_local_percent'] = 100*sum(bool(v & 4) and r['shadowed']==8 for r,v in zip(measured,masks))/len(masks)
    for start in range(0, seconds, 60):
        block=[r['frame_ms'] for r in clean if warmup+start <= r['seconds'] < warmup+min(start+60, seconds)]
        if block: result['minutes'].append(dict(start=start, **distribution(block, cap)))
    return result


def process_memory(handle):
    class Counters(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('faults', wintypes.DWORD)] + [
            (name, ctypes.c_size_t) for name in ('peak_rss','rss','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private')]
    counters= Counters(); counters.cb=ctypes.sizeof(counters)
    get=ctypes.WinDLL('psapi').GetProcessMemoryInfo
    get.argtypes=[wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD]
    if not get(wintypes.HANDLE(handle), ctypes.byref(counters), counters.cb):
        raise ctypes.WinError()
    return dict(rss=counters.rss, private=counters.private, faults=counters.faults)


def memory_window_mib(samples, start, end=float('inf')):
    values = [m['private'] for m in samples if start <= m['seconds'] < end]
    return statistics.mean(values)/2**20 if values else None


def safe(path, out):
    assert path.resolve().is_relative_to(out) and path.resolve()!=out
    for parent in [path, *path.parents]:
        if parent==out: break
        assert not parent.is_symlink() and not parent.is_junction(), parent
    return path


def cleanup(out):
    for path in out.rglob('*.map'): safe(path, out).unlink()
    saves=[p for p in (out/'saves').rglob('*') if p.is_file()]
    if saves:
        manifest=[dict(path=p.relative_to(out).as_posix(), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in saves]
        with ZipFile(out/'test-saves.zip', 'x', ZIP_DEFLATED, compresslevel=9) as archive:
            for path, info in zip(saves, manifest): archive.write(path, info['path'])
            archive.writestr('manifest.json', json.dumps(manifest, indent=2))
        with ZipFile(out/'test-saves.zip') as archive:
            for info in manifest: assert hashlib.sha256(archive.read(info['path'])).hexdigest()==info['sha256']
        for path in saves: safe(path, out).unlink()
    from PIL import Image
    for path in (out/'screenshots').glob('*.bmp'):
        target=path.with_suffix('.png')
        with Image.open(path) as source:
            source.save(target)
            with Image.open(target) as saved:
                assert saved.size == source.size and saved.mode == source.mode
                assert saved.tobytes() == source.tobytes()
        safe(path, out).unlink()
    for name in ['halo-performance.exe', 'SDL3.dll']:
        path=out/name
        if path.exists(): safe(path, out).unlink()


def verify_fixture_comparison(out, mode, log):
    from PIL import Image, ImageChops, ImageStat
    frames = sorted(p for p in (out/'screenshots').iterdir() if p.suffix.lower() in ('.bmp', '.png'))
    assert len(frames) == 7 and 'E3 comparison 6 mask 0' in log, 'Incomplete fixture comparison'
    # E1 measured the actual lamp markers. Reject accidental selection of
    # other emissive surfaces even when the frame reports two fixture slots.
    positions = {int(slot): tuple(map(float, (x,y,z))) for slot,x,y,z in
                 re.findall(r'E3 slot (\d) at ([-.\d]+) ([-.\d]+) ([-.\d]+)', log)}
    expected = {0:(7.6693,-10.9724,-.7990), 1:(9.9964,-11.0341,-.8103)}
    if mode == 'panel':
        expected = {0:(-8.5266,-12.3946,-2.2547)}
    for slot, reference in expected.items():
        assert slot in positions and math.dist(positions[slot], reference)<.03, 'Wrong fixture selected'
    assert 2 in positions, 'Missing stock flashlight'
    if mode == 'beam':
        assert re.search(r'E3 receiver vehicle (?!-1\b)-?\d+ rifle (?!-1\b)-?\d+', log)
        # The matched wall patch above the helmet is a BSP receiver behind
        # the caster. The stock beam stays lit in both frozen frames.
        if 'effective 1 active 1' not in log:
            with Image.open(frames[0]) as off, Image.open(frames[1]) as on:
                w, h = off.size
                wall = (round(w*600/1280), round(h*235/720), round(w*667/1280), round(h*267/720))
                delta = ImageChops.subtract(off.convert('RGB').crop(wall), on.convert('RGB').crop(wall)).convert('L')
                assert sum(delta.histogram()[20:]) > delta.width*delta.height*.15, 'Missing flashlight silhouette on the wall'
    if mode == 'comparison':
        floor = []
        for path in frames:
            with Image.open(path) as frame:
                w,h = frame.size
                floor.append(frame.convert('RGB').crop((int(w*.32),int(h*.51),int(w*.68),int(h*.79))))
        masked = [ImageChops.subtract(floor[0], image) for image in floor]
        energy = [sum(ImageStat.Stat(image).sum) for image in masked]
        assert min(energy[1:3])>4*(1+energy[6]), 'Individual lamp shadows must exceed the return-to-stock difference'
        assert energy[3]>1.25*max(energy[1:3]), 'Both fixture shadows must contribute'
        assert sum(ImageStat.Stat(ImageChops.difference(masked[1],masked[2])).sum)>.5*min(energy[1:3]), 'Lamp directions must differ'


def runtime_errors(log):
    # A resource fallback can leave the game running faster with no shadows.
    # Treat it as failed acceptance, even when earlier frames had valid maps.
    return re.findall(r'(?im)^.*(?:crash: exception|exception halt|assertion failed|GL_INVALID|shader.*(?:error|failed)|GPU:.*shadow.*(?:unavailable|failed)).*$', log)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root', type=Path, required=True)
    parser.add_argument('--settings', type=Path)
    parser.add_argument('--fps', type=int, choices=[30,60,120,240,360], required=True)
    parser.add_argument('--performance-cores-only', action='store_true', help='Request the Windows client startup policy')
    parser.add_argument('--foreground', action='store_true', help='Use a visible window and record actual foreground ownership')
    parser.add_argument('--uncapped', action='store_true', help='Measure headroom without the limiter; --fps remains the reporting budget')
    parser.add_argument('--without-environment-model', action='store_true', help='Diagnostic control build only: disable environmental model self-shadowing')
    parser.add_argument('--alternate-environment-model', action='store_true', help='Diagnostic control build only: alternate environmental model self-shadowing in two-second ABBA blocks')
    parser.add_argument('--depth-reuse', choices=['shared','legacy','alternate'], help='Requires depth-reuse control build; alternate uses two-second ABBA blocks')
    parser.add_argument('--enabled', type=int, choices=[0,1], required=True)
    parser.add_argument('--modern-shadows', choices=['off', 'hard', 'soft'], default='off')
    parser.add_argument('--extra-flashlight-slot', action='store_true', help='Allow eight other shadowed lights alongside the flashlight')
    parser.add_argument('--stock-flashlight', action='store_true', help='Keep the stock beam on in a shadow-off performance control')
    parser.add_argument('--shoulder-view', action='store_true', help='Exercise the stock shoulder-view light submission in a compatible local session')
    parser.add_argument('--fixture-room', choices=['timing','comparison','panel','beam'], help='Use the fixed two-lamp camera and caster on Rat Race')
    parser.add_argument('--environment-fixtures', action='store_true')
    parser.add_argument('--flashlight-shadows', action='store_true')
    parser.add_argument('--environment-sun', action='store_true')
    parser.add_argument('--turret-sources', type=int, choices=[1,2,4], help='Requires a turret-prototype timing build')
    parser.add_argument('--seconds', type=int, default=300)
    parser.add_argument('--warmup', type=int, default=30)
    parser.add_argument('--resolution', default='2560x1440')
    parser.add_argument('--map', default='carousel', choices=['carousel','bloodgulch','c40','ratrace'])
    parser.add_argument('--checkpoint-root', type=Path, help='Copy an existing campaign profile/save root; never run against the original')
    parser.add_argument('--invulnerable-player', action='store_true', help='Explicit controlled campaign stress run: keep the copied player alive; report separately from normal gameplay')
    parser.add_argument('--combat-core', type=Path, help='Restore the E5 snowy fight and fixed camera from a trusted local core save')
    parser.add_argument('--snowy-route', action='store_true', help='Walk the archived exterior-door checkpoint into the c40 snowy fight; test route only')
    parser.add_argument('--stationary-shield', action='store_true', help='Isolated stationary emitter, no bots or other enhanced lights')
    parser.add_argument('--advance-seconds', type=float, default=0, help='Walk forward in the copied campaign checkpoint with normal controller input')
    parser.add_argument('--turn-seconds', type=float, default=0, help='Turn after walking or the snowy route; negative turns left, positive right')
    parser.add_argument('--route-captures', action='store_true', help='Route inspection only: capture every two seconds instead of once a minute')
    parser.add_argument('--route-file', type=Path, help='Validated test-only controller steps: seconds lx ly rx use; requires --snowy-route')
    parser.add_argument('--bots', type=int, default=15)
    parser.add_argument('--label', required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/lighting-performance-build')
    parser.add_argument('--dump-shaders', action='store_true', help='Retain GLSL instead of screenshots for cache diagnosis; not an FPS acceptance run')
    parser.add_argument('--no-captures', action='store_true', help='Disable screenshot allocations/readback for memory and stall diagnostics')
    parser.add_argument('--shader-cache-from', type=Path, help='Warm-cache diagnostic: copy only shader binaries from a previous test-saves.zip')
    args=parser.parse_args()
    if (args.map == 'c40') != bool(args.checkpoint_root):
        parser.error('c40 requires --checkpoint-root; bot maps do not use a campaign save')
    if args.combat_core and (not args.checkpoint_root or not args.combat_core.is_file() or args.warmup < 20 or
                             args.snowy_route or args.advance_seconds or not args.invulnerable_player):
        parser.error('--combat-core requires a local core, a copied c40 checkpoint, a protected player, 20s warmup and a fixed camera')
    if args.invulnerable_player and not args.checkpoint_root:
        parser.error('--invulnerable-player requires a copied campaign checkpoint')
    if args.snowy_route and (not args.checkpoint_root or args.advance_seconds):
        parser.error('--snowy-route requires a copied campaign checkpoint and cannot combine with advance')
    if args.route_file:
        if not args.snowy_route:
            parser.error('--route-file requires --snowy-route')
        route = [line.split() for line in args.route_file.read_text().splitlines() if line.strip()]
        if not 1 <= len(route) <= 128:
            parser.error('Route must contain 1-128 steps')
        for step in route:
            if len(step) != 5 or not 0 < float(step[0]) <= 30 or any(not -32767 <= int(x) <= 32767 for x in step[1:4]) or step[4] not in tuple(str(i) for i in range(8)):
                parser.error('Invalid route step')
    if args.stationary_shield and args.map != 'c40':
        parser.error('--stationary-shield requires c40, which contains the shield tag')
    if args.advance_seconds and (args.map != 'c40' or not 0 < args.advance_seconds <= 30):
        parser.error('--advance-seconds requires c40 and a duration up to 30 seconds')
    if args.turn_seconds and (not (args.advance_seconds or args.snowy_route) or not 0 < abs(args.turn_seconds) <= 3):
        parser.error('--turn-seconds requires --advance-seconds or --snowy-route and a magnitude up to three seconds')
    assert re.fullmatch(r'[A-Za-z0-9_-]+', args.label)
    assert re.fullmatch(r'[1-9][0-9]*x[1-9][0-9]*', args.resolution)
    assert args.seconds>=30 and args.warmup>=0 and 1<=args.bots<=15
    out=(ROOT/'build/lighting-performance'/args.label).resolve()
    assert out.is_relative_to((ROOT/'build').resolve())
    out.mkdir(parents=True, exist_ok=False)
    (out/'maps').mkdir(); (out/'screenshots').mkdir()
    env={k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
    env['PATH']=str(ROOT/'build/windows')+os.pathsep+env.get('PATH','')
    settings={}
    if args.settings:
        display=tomllib.loads(args.settings.read_text())['display']
        settings={k:display[k] for k in QUALITY if k in display}
    settings.update({k:bool(args.enabled) for k in LIGHTS})
    settings['modern_shadows'] = args.modern_shadows
    if args.performance_cores_only:
        settings['performance_cores_only'] = True
    if args.shoulder_view:
        settings['third_person'] = True
        env['HALO_SESSION_COMPATIBILITY'] = 'gulchce_third_person'
    if args.environment_fixtures:
        settings['environment_fixtures'] = True
    if args.flashlight_shadows:
        settings['flashlight_shadows'] = True
        settings['flashlight_extra_slot'] = args.extra_flashlight_slot
    if args.flashlight_shadows or args.stock_flashlight:
        env['HALO_LIGHTING_BENCH_FLASHLIGHT'] = '1'
    if args.environment_sun:
        settings['environment_sun'] = True
    # These are deliberate test settings, not a player's legacy preferences.
    settings.update(dynamic_lighting_reset=True, overshield_effect=bool(args.enabled), shield_glow_intensity='default')
    if args.stationary_shield:
        settings.update({k:False for k in LIGHTS})
        settings.update(covenant_shield_lighting=bool(args.enabled), plasma_occlusion=bool(args.enabled), overshield_effect=False)
    (out/'config.toml').write_text('[display]\n'+''.join(f'{k} = {json.dumps(v)}\n' for k,v in settings.items()))
    env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='0' if args.foreground else '1',
               HALO_WINDOW_SIZE=args.resolution, HALO_RESOLUTION_SCALING='native', HALO_NO_AUDIO='1',
               HALO_MAX_FPS='-1' if args.uncapped else str(args.fps), HALO_NO_VSYNC='1', HALO_EXIT_AFTER=str(args.warmup+args.seconds+25),
               HALO_NET_ONLINE='0', HALO_NET_PUBLIC_LOBBY='0', HALO_NET_ALLOW_UPNP='0',
               HALO_NET_JOIN_FROM_CLIPBOARD='0',
               HALO_NET_ADDRESS=f'127.0.0.{81+args.enabled}', HALO_NET_BROADCAST=f'127.0.0.{81+args.enabled}',
               HALO_UPDATE_ANSWER='never', HALO_CRASH_REPORTS='no', HALO_NETWORK_TEST=f'local:{args.map}:slayer',
               HALO_NETWORK_TEST_START='5', HALO_NETWORK_TEST_SCORE='10000', HALO_BOTS=str(args.bots),
               HALO_BOT_SKILL='heroic', HALO_TEST_INPUT='bot:1', HALO_SCREENSHOT_DIR=str(out/'screenshots'))
    if args.foreground:
        env['HALO_DISPLAY_MODE'] = 'windowed'
    if args.checkpoint_root:
        for key in ['HALO_NETWORK_TEST', 'HALO_NETWORK_TEST_START', 'HALO_NETWORK_TEST_SCORE', 'HALO_BOTS', 'HALO_BOT_SKILL']:
            env.pop(key, None)
        env.update(HALO_MENU_OPEN='main_menu/campaign_select/campaign_menu',
                   HALO_TEST_INPUT='menu:'+','.join(['wait']*8+['a']),
                   HALO_EXIT_AFTER=str(args.warmup+args.seconds+25))
    if args.fixture_room in ('comparison', 'panel', 'beam'):
        env['HALO_EXIT_AFTER'] = '38'
    if args.stationary_shield:
        env.update(HALO_BOTS='0', HALO_LIGHTING_BENCH_SHIELD='1')
    if args.invulnerable_player:
        env['HALO_LIGHTING_BENCH_DEATHLESS'] = '1'
    if args.snowy_route:
        env['HALO_LIGHTING_BENCH_SNOWY_ROUTE'] = '1'
    if args.route_file:
        shutil.copy2(args.route_file, out/'controller-route.txt')
        env['HALO_LIGHTING_BENCH_ROUTE_FILE'] = 'controller-route.txt'
    if args.advance_seconds:
        env['HALO_LIGHTING_BENCH_ADVANCE'] = str(args.advance_seconds)
    if args.turn_seconds:
        env['HALO_LIGHTING_BENCH_TURN'] = str(args.turn_seconds)
    if args.route_captures:
        env['HALO_LIGHTING_BENCH_ROUTE_CAPTURES'] = '1'
    if args.dump_shaders:
        (out/'shaders').mkdir()
        env['HALO_GPU_DUMP_SHADERS']=str(out/'shaders')
        # Diagnose cache growth without periodic readback/large image allocations.
        env['HALO_SCREENSHOT_DIR']=''
    if args.no_captures:
        env['HALO_SCREENSHOT_DIR']=''
    build=args.build_dir.resolve()
    if args.fixture_room:
        assert args.map=='ratrace' and json.loads((build/'manifest.json').read_text()).get('fixture_room'), 'Requires the Rat Race fixture build'
        env['HALO_BENCH_FIXTURE_ROOM'] = args.fixture_room
        env['HALO_VIEWMODEL_VISIBLE'] = '0'

    if args.turret_sources:
        assert json.loads((build/'manifest.json').read_text()).get('turret_prototype'), 'Wrong timing build'
        env['HALO_TURRET_SOURCES']=str(args.turret_sources)
    assert build.is_relative_to((ROOT/'build').resolve())
    shutil.copy2(build/'manifest.json', out/'build-manifest.json')
    if args.without_environment_model:
        assert json.loads((build/'manifest.json').read_text()).get('environment_model_control'), 'Control build required'
        env['HALO_BENCH_NO_ENV_MODEL'] = '1'
    else:
        env.pop('HALO_BENCH_NO_ENV_MODEL', None)
    if args.alternate_environment_model:
        assert not args.without_environment_model, 'Choose one environment comparison mode'
        assert json.loads((build/'manifest.json').read_text()).get('environment_model_control'), 'Control build required'
        env['HALO_BENCH_ENV_ALTERNATE'] = '1'
    else:
        env.pop('HALO_BENCH_ENV_ALTERNATE', None)
    env.pop('HALO_BENCH_DEPTH_REUSE', None)
    if args.depth_reuse:
        assert not args.alternate_environment_model and not args.without_environment_model, 'Choose one comparison'
        assert json.loads((build/'manifest.json').read_text()).get('depth_reuse_control'), 'Depth reuse control build required'
        env['HALO_BENCH_DEPTH_REUSE'] = args.depth_reuse
    (out/'settings.json').write_text(json.dumps(dict(resolution=args.resolution, fps=args.fps, uncapped=args.uncapped,
        depth_reuse=args.depth_reuse, environment_model_enabled=not args.without_environment_model,
        environment_model_alternating=args.alternate_environment_model,
        frame_limit=int(env['HALO_MAX_FPS']), turret_sources=args.turret_sources,
        warmup=args.warmup, seconds=args.seconds, bots=0 if args.checkpoint_root else args.bots, map=args.map,
        stationary_shield=args.stationary_shield,
        campaign_advance_seconds=args.advance_seconds, campaign_turn_seconds=args.turn_seconds, route_captures=args.route_captures,
        invulnerable_player=args.invulnerable_player,
        snowy_route=args.snowy_route,
        shader_dump=args.dump_shaders, captures=not (args.dump_shaders or args.no_captures), display=settings),indent=2))
    if args.shader_cache_from:
        copied = []
        with ZipFile(args.shader_cache_from) as archive:
            for name in archive.namelist():
                if not re.fullmatch(r'saves/shader_cache/[0-9a-fA-F]+\.bin', name): continue
                target = safe(out/name, out)
                target.parent.mkdir(parents=True, exist_ok=True)
                content = archive.read(name)
                target.write_bytes(content)
                copied.append(dict(path=name, sha256=hashlib.sha256(content).hexdigest()))
        assert copied, 'No shader binaries in the supplied test archive'
        (out/'shader-cache-source.json').write_text(json.dumps(dict(source=str(args.shader_cache_from.resolve()), files=copied), indent=2))
    memory=[]; start=time.monotonic(); process=None
    try:
        if args.checkpoint_root:
            source = args.checkpoint_root.resolve()
            manifest = []
            for path in (source/'u').rglob('*'):
                if not path.is_file(): continue
                relative = path.relative_to(source)
                target = safe(out/'saves'/relative, out)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
                manifest.append(dict(path=relative.as_posix(), sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
            assert any(item['path'].endswith('/savegame.bin') for item in manifest), 'No campaign checkpoint found'
            (out/'checkpoint-source.json').write_text(json.dumps(dict(source=str(source), files=manifest), indent=2))
        for name in ['ui.map', args.map+'.map']:
            shutil.copy2(args.data_root/'maps'/name, out/'maps'/name)
        # Desktop configuration is next to the executable, not the data root.
        shutil.copy2(build/'halo-performance.exe', out/'halo-performance.exe')
        dll = build/'SDL3.dll'
        shutil.copy2(dll if dll.exists() else ROOT/'build/windows/SDL3.dll', out/'SDL3.dll')
        if args.combat_core:
            import lighting_combat_core as combat
            port, core_hash = combat.prepare(out, env, args.combat_core)
            with (out/'maps'/f'{args.map}.map').open('rb') as map_file:
                map_hash = hashlib.file_digest(map_file, 'sha256').hexdigest()
        with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
            process=subprocess.Popen([str(out/'halo-performance.exe')], cwd=out, env=env,
                stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
            (out/'process.json').write_text(json.dumps(dict(pid=process.pid, executable=str(out/'halo-performance.exe'))))
            staging = None
            try:
                if args.combat_core:
                    staging = ThreadPoolExecutor(max_workers=1)
                    staged = staging.submit(combat.play, out, process, port)
                while process.poll() is None:
                    if staging and staged.done():
                        staged.result()
                    if time.monotonic()-start>args.warmup+args.seconds+120:
                        raise TimeoutError('Game did not exit')
                    memory.append(dict(seconds=time.monotonic()-start, **process_memory(process._handle)))
                    if args.foreground:
                        foreground_pid = wintypes.DWORD()
                        user32 = ctypes.windll.user32
                        user32.GetForegroundWindow.restype = wintypes.HWND
                        user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
                        user32.GetWindowThreadProcessId(user32.GetForegroundWindow(), ctypes.byref(foreground_pid))
                        memory[-1]['foreground'] = foreground_pid.value == process.pid
                        timer = out/'frames.csv'
                        if timer.exists():
                            with timer.open('rb') as stream:
                                stream.seek(max(0, stream.seek(0, 2) - 512))
                                lines = stream.read().split(b'\n')
                            for line in reversed(lines[:-1]):
                                field = line.split(b',', 1)[0]
                                if field.replace(b'.', b'', 1).isdigit():
                                    memory[-1]['game_seconds'] = float(field)
                                    break
                    if os.name == "nt":
                        allowed, system = ctypes.c_size_t(), ctypes.c_size_t()
                        if ctypes.windll.kernel32.GetProcessAffinityMask(wintypes.HANDLE(process._handle), ctypes.byref(allowed), ctypes.byref(system)):
                            memory[-1]['affinity_mask'] = hex(allowed.value)
                    (out/'memory.json').write_text(json.dumps(memory,indent=2))
                    time.sleep(5)
            finally:
                if process.poll() is None: process.terminate(); process.wait(10)
                if staging:
                    staging.shutdown()
                    staged.result()
        rows=[]
        if (out/'frames.csv').exists():
            with (out/'frames.csv').open() as frames:
                rows=[{k:float(v) for k,v in row.items()} for row in csv.DictReader(frames)]
        report={}
        try:
            report=summarize(rows,args.warmup,args.seconds,args.fps,
                             captures=not (args.dump_shaders or args.no_captures))
        except ValueError as error:
            report['measurement_error']=str(error)
        log=(out/'stderr.log').read_text(errors='replace')
        report['performance_cores_only'] = args.performance_cores_only
        if rows and 'processor' in rows[0]:
            placement = {}
            for row in rows:
                if args.warmup <= row['seconds'] < args.warmup + args.seconds:
                    cpu = int(row['processor'])
                    placement[cpu] = placement.get(cpu, 0) + 1
            report['frame_end_processors'] = placement

        if args.foreground:
            report['foreground_observations'] = [m['foreground'] for m in memory if args.warmup <= m.get('game_seconds', -1) < args.warmup + args.seconds]
        if args.route_file and f'BENCH custom route steps {len(route)}\n' not in log:
            report['route_error'] = 'Requested controller route was not loaded completely'
        report.update(exit=process.returncode, fps=args.fps, uncapped=args.uncapped, frame_limit=int(env['HALO_MAX_FPS']), enabled=args.enabled, shader_dump=args.dump_shaders,
                      modern_shadows=args.modern_shadows, environment_sun=args.environment_sun, resolution=args.resolution,
                      environment_fixtures=args.environment_fixtures, flashlight_shadows=args.flashlight_shadows,
                      flashlight_extra_slot=args.extra_flashlight_slot,
                      invulnerable_player=args.invulnerable_player, snowy_route=args.snowy_route,
                      route_sha256=hashlib.sha256((out/'controller-route.txt').read_bytes()).hexdigest() if args.route_file else None,
                      captures=not (args.dump_shaders or args.no_captures),
                      unexpected_players=bool(re.search(r'\bplayer '+str(args.bots+1)+r':', log)),
                      bots_lines=[line for line in log.splitlines() if 'bots: tick' in line],
                      errors=runtime_errors(log),
                      peak_private_mib=max((m['private']/2**20 for m in memory),default=None),
                      peak_rss_mib=max((m['rss']/2**20 for m in memory),default=None),
                      # Memory uses the runner clock. Bound both windows before
                      # the exit deadline; shutdown deallocation is not a plateau.
                      first_minute_private_mib=memory_window_mib(memory,args.warmup+15,min(args.warmup+75,args.warmup+args.seconds)),
                      last_minute_private_mib=memory_window_mib(memory,max(args.warmup+15,args.warmup+args.seconds-60),args.warmup+args.seconds))
        if args.combat_core:
            report['map_sha256'] = map_hash
            report['combat_core_sha256'] = core_hash
            ticks = [int(row['tick']) for row in rows if args.warmup <= row['seconds'] < args.warmup + args.seconds]
            if not ticks:
                raise ValueError('No combat simulation ticks in the measurement window')
            report['combat_tick_range'] = [ticks[0], ticks[-1]]
            report['combat_squad_counts'] = combat.verify(out, log, args.warmup, args.seconds, report['combat_tick_range'])
            # Only the verified local duplicate is disposable.
            copied = out/'core/canyon.bin'
            if hashlib.sha256(copied.read_bytes()).hexdigest() == hashlib.sha256(args.combat_core.read_bytes()).hexdigest():
                safe(copied, out).unlink()
        (out/'report.json').write_text(json.dumps(report,indent=2))
        print(json.dumps(dict(label=args.label, **report)),flush=True)
        assert report['exit']==0 and not report['errors'], 'Game failed; retained report and logs describe the failure'
        if args.fixture_room in ('comparison','panel','beam'):
            verify_fixture_comparison(out, args.fixture_room, log)
            if args.shoulder_view:
                assert 'effective 1 active 1' in log, 'Shoulder camera never activated'
            # The reference pose uses a frozen simulation. Its frames do not
            # belong in an FPS acceptance result.
            return
        if args.fixture_room=='timing':
            assert report.get('both_fixture_percent',0)>0, 'The two fixture maps never coexisted'
        if args.flashlight_shadows:
            assert report.get('flashlight_percent',0)>0, 'No live flashlight depth measured'
            if not args.extra_flashlight_slot and json.loads((build/'manifest.json').read_text()).get('shared_flashlight_budget'):
                assert report['peak_local_shadows_including_beam']<=8, 'Flashlight exceeded the eight-local-shadow budget'
        assert not report.get('route_error'), report.get('route_error')
        assert not report.get('measurement_error'), report.get('measurement_error')
        # A real stall at the beginning or end can shorten the span between
        # in-window samples while the frame clock still covers the interval.
        complete = report['measured_span'] >= args.seconds - .2 if report['captures'] else 'sustained' in report
        assert complete, 'Incomplete measurement'
        assert not report['unexpected_players'], 'Unexpected player joined the isolated workload'
        if args.performance_cores_only:
            assert 'CPU scheduling: performance cores only (mask ' in log, 'Requested process affinity did not take effect'
        assert bool(report['selected_max'])==bool(args.enabled), 'Light activation mismatch'
        if args.modern_shadows != 'off' and args.enabled:
            assert report.get('shadowed_frame_percent', 0) > 0, 'No active shadow maps measured'
        elif 'shadowed_frame_percent' in report:
            assert report['shadowed_frame_percent'] == 0, 'Unexpected active shadow maps'
        if args.environment_sun and args.snowy_route and args.modern_shadows in ('hard','soft'):
            assert report.get('sun_frame_percent',0)>0, 'No sun shadows measured during the snowy route'
        if args.invulnerable_player:
            assert 'BENCH controlled invulnerable player enabled' in log, 'Controlled player protection was not active'
        if args.stationary_shield:
            assert re.search(r'BENCH shield created (?!-1\b)-?\d+', log), 'Stationary emitter was not created'
        if args.seconds>=120 and not args.checkpoint_root and not args.stationary_shield: assert len(report['bots_lines'])>=args.seconds//60-1, 'Insufficient combat telemetry'
    finally:
        cleanup(out)


if __name__=='__main__':
    main()

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
