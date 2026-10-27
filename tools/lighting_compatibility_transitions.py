"""Exercise Xbox -> MCC -> CE -> Xbox in one isolated Windows process.

Requires local retail data and an existing profile folder; neither is committed.
Build the passive probe with lighting_compatibility_build.py first.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

from lighting_compatibility_run import ROOT
from lighting_performance_run import cleanup, LIGHTS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root', type=Path, required=True)
    parser.add_argument('--profile-root', type=Path, required=True)
    parser.add_argument('--enabled', type=int, choices=(0, 1), required=True)
    args = parser.parse_args()
    enabled = args.enabled
    out = (ROOT/'build/lighting-compatibility'/f'transitions-{enabled}').resolve()
    assert out.is_relative_to((ROOT/'build').resolve())
    out.mkdir(parents=True, exist_ok=False)
    build = ROOT/'build/lighting-compatibility-build'
    manifest = json.loads((build/'manifest.json').read_text())
    assert hashlib.sha256((build/'halo-compatibility.exe').read_bytes()).hexdigest() == manifest['executable_sha256']
    process = connection = None
    transcript = ''
    sent = []
    commands = [(30, '(map_name "mcc_maps\\Mercury 1 & 2\\mercury_falling")'),
                (64, '(map_name custom_maps\\deathisland)'),
                (90, '(map_name levels\\test\\bloodgulch\\bloodgulch)')]
    try:
        for folder, names in [('maps', ['ui', 'c40', 'bloodgulch']),
                              ('custom_maps', ['deathisland', 'bitmaps', 'sounds', 'loc']),
                              ('mcc_maps/Mercury 1 & 2', ['mercury_falling', 'bitmaps', 'sounds'])]:
            for name in names:
                target = out/folder/(name+'.map')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(args.data_root/folder/(name+'.map'), target)
        assert args.profile_root.is_dir()
        for src in args.profile_root.rglob('*'):
            if src.is_file() and src.suffix != '.map':
                target = out/'saves'/src.relative_to(args.profile_root)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, target)
        (out/'screenshots').mkdir()
        shutil.copy2(build/'halo-compatibility.exe', out/'halo-performance.exe')
        shutil.copy2(build/'manifest.json', out/'build-manifest.json')
        shutil.copy2(ROOT/'build/windows/SDL3.dll', out/'SDL3.dll')
        settings = {key: bool(enabled) for key in LIGHTS}
        settings.update(overshield_effect=bool(enabled), per_pixel_lighting=True, shield_glow_intensity='default')
        (out/'config.toml').write_text('[display]\n'+''.join(f'{k} = {json.dumps(v)}\n' for k, v in settings.items()))
        (out/'init.txt').write_text('map_name levels\\c40\\c40\n')
        port = 24041+enabled
        env = {k: v for k, v in os.environ.items() if not k.startswith('HALO_')}
        env.update(HALO_DATA_ROOT=str(out), HALO_SAVE_ROOT=str(out/'saves'), HALO_HIDDEN_WINDOW='1',
                   HALO_NO_AUDIO='1', HALO_MAX_FPS='60', HALO_NO_VSYNC='1', HALO_EXIT_AFTER='118',
                   HALO_NET_ONLINE='0', HALO_NET_PUBLIC_LOBBY='0', HALO_NET_JOIN_FROM_CLIPBOARD='0',
                   HALO_NET_ALLOW_UPNP='0', HALO_UPDATE_ANSWER='never', HALO_CRASH_REPORTS='no',
                   HALO_TELNET_CONSOLE='1', HALO_TELNET_CONSOLE_PORT=str(port), HALO_WINDOW_SIZE='1280x720',
                   HALO_SCREENSHOT_DIR=str(out/'screenshots'), HALO_SCREENSHOT_EVERY='900', HALO_NETWORK_TEST='log')
        (out/'test-settings.json').write_text(json.dumps(dict(display=settings,
            environment={k: v for k, v in env.items() if k.startswith('HALO_')}), indent=2))
        start = time.monotonic()
        with (out/'stdout.log').open('wb') as stdout, (out/'stderr.log').open('wb') as stderr:
            process = subprocess.Popen([str(out/'halo-performance.exe')], cwd=out, env=env,
                                       stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
            while process.poll() is None and time.monotonic()-start < 150:
                try:
                    if connection is None:
                        connection = socket.create_connection(('127.0.0.1', port), .1)
                        connection.settimeout(.01)
                    transcript += connection.recv(65536).decode('latin1')
                except OSError:
                    pass
                if connection:
                    for at, command in commands:
                        if time.monotonic()-start >= at and at not in sent:
                            connection.sendall((command+'\r\n').encode())
                            sent.append(at)
                time.sleep(.15)
    finally:
        if connection:
            connection.close()
        forced = process is not None and process.poll() is None
        if forced:
            process.terminate()
            process.wait(10)
        (out/'console.log').write_text(transcript)
        cleanup(out)
    log = (out/'debug.txt').read_text(errors='replace')
    with (out/'lighting-policy.csv').open() as stream:
        rows = [{k: int(v) for k, v in row.items()} for row in csv.DictReader(stream)]
    report = dict(enabled=enabled, exit=process.returncode, forced=forced, commands=sent,
                  mcc_loads=log.count('mcc: loaded'), ce_loads=log.count('custom edition: loading'),
                  tick_resets=sum(b['tick'] < a['tick'] for a, b in zip(rows, rows[1:])),
                  policy_values=sorted({r['policy'] for r in rows}),
                  shield_values=sorted({r['shield_enabled'] for r in rows}),
                  screenshots=len(list((out/'screenshots').glob('*.png'))), exceptions=log.count('EXCEPTION'))
    report['passed'] = (report['exit'] == 0 and not forced and len(sent) == 3 and
                        report['mcc_loads'] == 1 and report['ce_loads'] == 1 and
                        report['tick_resets'] >= 3 and report['shield_values'] == [enabled] and not report['exceptions'])
    (out/'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
