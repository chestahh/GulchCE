"""Observe an unchanged Windows baseline with two isolated, hidden peers.

Investigation runner, not a gameplay acceptance test. Uses existing network_test
controls and net_compare's judge; no engine instrumentation or retail assets.
Each output directory must be new. No automatic cleanup of evidence or saves.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import net_compare


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class MemoryCounters(ctypes.Structure):
    _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
        (name, ctypes.c_size_t) for name in ('peak_working', 'working',
        'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'pagefile',
        'peak_pagefile', 'private')]


def memory(process):
    result = MemoryCounters()
    result.cb = ctypes.sizeof(result)
    function = ctypes.windll.psapi.GetProcessMemoryInfo
    function.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
    if function(int(process._handle), ctypes.byref(result), result.cb):
        return {'working_bytes': result.working, 'private_bytes': result.private}
    return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--maps', type=Path, required=True)
    parser.add_argument('--profile', type=Path, required=True, help='Existing blam.sav; copied only')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--level', default='levels\\a30\\a30')
    parser.add_argument('--family', choices=['xbox', 'ce', 'mcc'], default='xbox')
    parser.add_argument('--extra-map', type=Path)
    parser.add_argument('--support-map', type=Path, action='append', default=[],
                        help='Additional Xbox map needed for a transition; copied into each private maps folder')
    parser.add_argument('--variant', default='coop')
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--late', type=int, default=0)
    parser.add_argument('--latency', type=int, default=0, help='Existing receive-delay setting in milliseconds per peer')
    parser.add_argument('--loss', type=int, default=0)
    parser.add_argument('--cues', default='')
    parser.add_argument('--record-profile', action='store_true')
    parser.add_argument('--events', action='store_true', help='Existing scripted hit/kill/vehicle/pickup controls')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This observer uses Windows process/memory APIs')
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    binary = args.binary.resolve()
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                'binary_sha256': digest(binary), 'arguments': vars(args), 'maps': {}, 'peers': {}}
    jobs = []
    observations = []
    started = time.monotonic()
    try:
        for index, role in enumerate(('host', 'client')):
            folder = root / role
            (folder / 'maps').mkdir(parents=True)
            shutil.copy2(binary, folder / 'halo.exe')
            shutil.copy2(binary.parent / 'SDL3.dll', folder / 'SDL3.dll')
            names = ['ui.map', 'carousel.map']
            if args.family == 'xbox':
                names.append(args.level.split('\\')[-1] + '.map')
            for name in sorted(set(names)):
                source = args.maps / name
                shutil.copy2(source, folder / 'maps' / name)
                if name not in manifest['maps']:
                    manifest['maps'][name] = {'bytes': source.stat().st_size, 'sha256': digest(source)}
            for source in args.support_map:
                target = folder / 'maps' / source.name
                if target.exists():
                    raise ValueError(f'Support map duplicates an existing fixture: {source.name}')
                shutil.copy2(source, target)
                manifest['maps'][source.name] = {'bytes': source.stat().st_size, 'sha256': digest(source)}
            if args.family != 'xbox':
                target = folder / ('custom_maps' if args.family == 'ce' else 'mcc_maps')
                target.mkdir()
                if not args.extra_map:
                    raise ValueError('--extra-map is required for CE/MCC')
                shutil.copy2(args.extra_map, target / args.extra_map.name)
                manifest['maps'][args.family + '/' + args.extra_map.name] = {
                    'bytes': args.extra_map.stat().st_size, 'sha256': digest(args.extra_map)}
                if args.family == 'ce':
                    for name in ('bitmaps.map', 'sounds.map', 'loc.map'):
                        source = args.extra_map.parent / name
                        shutil.copy2(source, target / name)
                        manifest['maps'][name] = {'bytes': source.stat().st_size, 'sha256': digest(source)}
            profile = folder / 'saves' / 'u' / 'UDATA' / args.profile.parent.name
            profile.mkdir(parents=True)
            shutil.copy2(args.profile, profile / 'blam.sav')
            (folder / 'config.toml').write_text('[display]\nfullscreen=false\n')
            env = {key: value for key, value in os.environ.items() if not key.startswith('HALO_')}
            env.update(SDL_VIDEO_DRIVER='dummy', SDL_AUDIO_DRIVER='dummy', HALO_NULL_RENDERER='1',
                       HALO_HIDDEN_WINDOW='1', HALO_NO_AUDIO='1', HALO_NET_ONLINE='0',
                       HALO_NET_PUBLIC_LOBBY='0', HALO_NET_ALLOW_UPNP='0', HALO_MAX_FPS='30',
                       HALO_NET_MAP_SHARE='0', HALO_NET_MAP_SEED='0', HALO_NET_MAP_DOWNLOAD='0',
                       HALO_NETWORK_TEST_PUBLIC='0', HALO_NET_JOIN_FROM_CLIPBOARD='0',
                       HALO_NETWORK_TEST=f'host:{args.level}:{args.variant}' if index == 0 else 'join',
                       HALO_NETWORK_TEST_START='10', HALO_NET_ADDRESS=f'127.0.0.{236+index}',
                       HALO_NET_BROADCAST=f'127.0.0.{237-index}', HALO_DATA_ROOT=str(folder),
                       HALO_SAVE_ROOT=str(folder / 'saves'), HALO_UPDATE_ANSWER='never',
                       HALO_UPDATE_AUTO='0', HALO_DISCORD_APPLICATION='',
                       HALO_SESSION_COMPATIBILITY='opence_crossplay', HALO_TEST_INPUT=f'bot:{1+index}',
                       HALO_EXIT_AFTER=str(args.seconds + args.late + 150),
                       HALO_NETWORK_LATENCY=str(args.latency), HALO_NETWORK_LOSS=str(args.loss),
                       HALO_NETWORK_TEST_CUE_PLAYERS='2', HALO_NETWORK_TEST_CUES=args.cues if not index else '',
                       HALO_CUSTOM_EDITION='1' if args.family == 'ce' else '0')
            if args.record_profile:
                env.update(HALO_PROFILE_RECORD='1', HALO_PROFILE_RECORD_WHEN='game', HALO_PROFILE_MEMORY='16')
            if args.events:
                env.update(HALO_NETWORK_TEST_SHOOT='4', HALO_NETWORK_TEST_KILL='17',
                           HALO_NETWORK_TEST_VEHICLE='20', HALO_NETWORK_TEST_PICKUP='30')
            if index and args.late:
                time.sleep(args.late)
            process = subprocess.Popen([str(folder / 'halo.exe')], cwd=folder, env=env,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            jobs.append((role, folder, process))
            manifest['peers'][role] = {'pid': process.pid, 'environment': {
                key: value for key, value in env.items() if key.startswith(('HALO_', 'SDL_'))}}
        deadline = time.monotonic() + args.seconds + 75
        ready_since = None
        while time.monotonic() < deadline:
            ready = []
            sample = {'elapsed_seconds': round(time.monotonic() - started, 3)}
            for role, folder, process in jobs:
                log = folder / 'halo.log'
                text = log.read_text(errors='replace') if log.exists() else ''
                lines = [line for line in text.splitlines() if 'network test: tick ' in line and 'player 1:' in line]
                ready.append(bool(lines))
                sample[role] = {'records': len(lines), **memory(process)}
            observations.append(sample)
            if all(ready) and ready_since is None:
                ready_since = time.monotonic()
            if ready_since and time.monotonic() - ready_since >= args.seconds:
                break
            if any(process.poll() is not None for _, _, process in jobs):
                break
            time.sleep(1)
    finally:
        cleanup_errors = []
        for role, folder, process in jobs:
            manifest['peers'][role]['early_exit'] = process.poll()
            try:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired) as error:
                # Still attempt to stop the other owned peer and retain evidence.
                if process.poll() is None:
                    cleanup_errors.append(f'{role}: {error}')
        manifest['cleanup_errors'] = cleanup_errors
        manifest['elapsed_seconds'] = time.monotonic() - started
        (root / 'manifest.json').write_text(json.dumps(manifest, indent=2, default=str))
        (root / 'observations.json').write_text(json.dumps(observations, indent=2))
        if cleanup_errors:
            raise RuntimeError('; '.join(cleanup_errors))
    result = net_compare.judge_run(root / 'host' / 'halo.log', root / 'client' / 'halo.log',
                                  SimpleNamespace(seconds=args.seconds, skip=3), net_compare.TOLERANCES)
    # A noisy campaign comparison is evidence, not a reason to weaken the judge.
    result['early_exits'] = {role: manifest['peers'][role]['early_exit'] for role in manifest['peers']}
    (root / 'comparison.json').write_text(json.dumps(result, indent=2, default=str))
    print(json.dumps(result, default=str))


if __name__ == '__main__':
    main()
