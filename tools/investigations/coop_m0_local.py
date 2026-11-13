"""Run up to two private Windows M0 fixtures with ordinary menu events/console.

A JSON specification supplies sources and environment overrides. Its optional
control file is a list of {id, at, peer, button/controller or command} actions.
Menu events require the generated M0 trace build. Console commands use the
existing loopback telnet console. No production implementation is changed.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    spec = json.loads(Path(sys.argv[1]).read_text(encoding='utf8'))
    if not 1 <= len(spec['peers']) <= 2:
        raise ValueError('M0 permits one or two game instances')
    output = Path(spec['output']).resolve()
    output.mkdir(parents=True, exist_ok=False)
    jobs, actions, folders = [], [], []
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                'runner_sha256': sha(Path(__file__)), 'specification': spec, 'peers': {}}
    buttons = dict(a=0, b=1, x=2, y=3, black=4, white=5, lt=6, rt=7,
                   up=8, down=9, left=10, right=11, start=12, back=13)
    started = time.monotonic()
    try:
        for number, peer in enumerate(spec['peers']):
            name = peer['name']
            folder = output / name
            if folder.parent != output or name in manifest['peers']:
                raise ValueError('Invalid or duplicate peer name')
            folder.mkdir()
            folders.append(folder)
            copied = []
            for destination, source_name in peer['files'].items():
                source, target = Path(source_name).resolve(), (folder / destination).resolve()
                if not target.is_relative_to(folder):
                    raise ValueError('Fixture destination escaped its peer')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
                copied.append({'source': str(source), 'destination': destination, 'sha256': sha(source)})
            binary = Path(peer['binary']).resolve()
            shutil.copy2(binary, folder / 'halo.exe')
            shutil.copy2(binary.parent / 'SDL3.dll', folder / 'SDL3.dll')
            (folder / 'config.toml').write_text('[display]\nfullscreen=false\n')
            (folder / 'captures').mkdir()
            if peer.get('init'):
                (folder / 'init.txt').write_text(peer['init'] + '\n')
            env = {k:v for k,v in os.environ.items() if not k.startswith(('HALO_', 'M0_', 'SDL_'))}
            env.update(HALO_DATA_ROOT=str(folder), HALO_SAVE_ROOT=str(folder/'saves'),
                HALO_HIDDEN_WINDOW='1', HALO_NO_AUDIO='1', HALO_MAX_FPS='30', HALO_NO_VSYNC='1',
                HALO_EXIT_AFTER=str(spec['seconds'] + 30), HALO_NET_ONLINE='0', HALO_NET_LISTINGS='brokers',
                HALO_NET_PUBLIC_LOBBY='0', HALO_NETWORK_TEST_PUBLIC='0', HALO_NET_ALLOW_UPNP='0',
                HALO_NET_MAP_SHARE='0', HALO_NET_MAP_DOWNLOAD='0', HALO_NET_JOIN_FROM_CLIPBOARD='0',
                HALO_UPDATE_ANSWER='never', HALO_UPDATE_AUTO='0', HALO_CRASH_REPORTS='no',
                HALO_NET_ADDRESS=f'127.0.0.{236+number}', HALO_NET_BROADCAST=f'127.0.0.{237-number}',
                HALO_DISCORD_APPLICATION='', HALO_SESSION_COMPATIBILITY='opence_crossplay',
                HALO_NETWORK_TEST='log', HALO_TELNET_CONSOLE='1', HALO_TELNET_CONSOLE_PORT=str(24320+number),
                HALO_WINDOW_SIZE='2560x1440', HALO_SCREENSHOT_DIR=str(folder/'captures'),
                HALO_SCREENSHOT_EVERY='180', M0_UI_INPUT='d:\\input.txt')
            env.update(peer.get('environment', {}))
            process = subprocess.Popen([str(folder/'halo.exe')], cwd=folder, env=env,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            job = {'name': name, 'folder': folder, 'process': process, 'connection': None,
                   'port': int(env['HALO_TELNET_CONSOLE_PORT']), 'transcript': bytearray()}
            jobs.append(job)
            manifest['peers'][name] = {'pid': process.pid, 'binary_sha256': sha(binary),
                                     'files': copied, 'environment': {k:v for k,v in env.items() if k.startswith(('HALO_', 'M0_', 'SDL_'))}}
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2))
        done = set()
        while time.monotonic() - started < spec['seconds']:
            elapsed = time.monotonic() - started
            for job in jobs:
                if job['process'].poll() is not None:
                    continue
                try:
                    if job['connection'] is None:
                        job['connection'] = socket.create_connection(('127.0.0.1', job['port']), .1)
                        job['connection'].settimeout(.001)
                    data = job['connection'].recv(65536)
                    if data: job['transcript'].extend(data)
                except (OSError, TimeoutError):
                    pass
            control = Path(spec['control']) if spec.get('control') else None
            if control and control.exists():
                try: requested = json.loads(control.read_text())
                except json.JSONDecodeError: requested = []
                for action in requested:
                    if action['id'] in done or action.get('at', 0) > elapsed: continue
                    job = next(j for j in jobs if j['name'] == action['peer'])
                    if 'button' in action:
                        target = job['folder']/'input.txt'
                        temporary = target.with_suffix('.tmp')
                        temporary.write_text(f"{action['id']} {action.get('controller',0)} {buttons[action['button']]}\n")
                        temporary.replace(target)
                    elif 'command' in action:
                        if job['connection'] is None: continue
                        job['connection'].sendall((action['command']+'\r\n').encode('ascii'))
                    else:
                        raise ValueError('Unrecognized fixture action')
                    done.add(action['id']); actions.append({'elapsed': elapsed, **action})
                    # Do not overwrite a menu event before the game consumes it.
                    time.sleep(.2)
            if (output/'stop').exists() or all(j['process'].poll() is not None for j in jobs): break
            time.sleep(.1)
    finally:
        for job in jobs:
            process = job['process']
            manifest['peers'][job['name']]['exit_before_cleanup'] = process.poll()
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=15)
            if job['connection']: job['connection'].close()
            (job['folder']/'console.log').write_bytes(job['transcript'])
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2))
        (output/'actions.json').write_text(json.dumps(actions, indent=2))
        # Private copies/caches only; saves and captures remain evidence.
        removed = 0
        for folder in folders:
            targets = [folder/n for n in ('maps','custom_maps','mcc_maps','halo.exe','SDL3.dll')]
            targets += list((folder/'saves/z').glob('cache*.map'))
            for target in targets:
                if not target.exists(): continue
                paths = [target, *target.rglob('*')] if target.is_dir() else [target]
                if any(not p.resolve().is_relative_to(output) or p.is_symlink() or p.is_junction() for p in paths):
                    raise ValueError('Unsafe private fixture cleanup path')
                removed += sum(p.stat().st_size for p in paths if p.is_file())
                if target.is_dir(): shutil.rmtree(target)
                else: target.unlink()
        (output/'cleanup.json').write_text(json.dumps({'removed_bytes': removed}))
    print(output)


if __name__ == '__main__':
    main()
