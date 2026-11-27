"""Stage the fixed E5 snowy combat camera and verify the engine loaded its core save."""
import hashlib
import re
import shutil
import socket
import time

CAMERA = '327.8 -649.3 74.5\n-0.99 -0.1 -0.06\n-0.06 0 0.9982\n1.22173\n'
# The E5 checkpoint restores tick 480. These triggers preserve the roughly
# two seconds of play before the restore and before switching to the saved camera.
LOAD_TICK = 62
CAMERA_TICK = 540


def _at_tick(console, tick, command):
    console.sendall(f'(begin (sleep_until (= (game_time) {tick}) 1) {command})\r\n'.encode())


def prepare(out, env, core):
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    env.pop('HALO_MENU_OPEN', None)
    env.pop('HALO_TEST_INPUT', None)
    env.update(HALO_TELNET_CONSOLE='1', HALO_TELNET_CONSOLE_PORT=str(port), HALO_NETWORK_TEST='log')
    (out / 'camera.txt').write_text(CAMERA)
    (out / 'init.txt').write_text('map_name levels\\c40\\c40\n')
    dest = out / 'core' / 'canyon.bin'
    dest.parent.mkdir()
    shutil.copyfile(core, dest)
    return port, hashlib.sha256(dest.read_bytes()).hexdigest()


def play(out, process, port):
    deadline = time.monotonic() + 35
    while process.poll() is None and time.monotonic() < deadline:
        try:
            console = socket.create_connection(('127.0.0.1', port), .3)
            break
        except OSError:
            time.sleep(.2)
    else:
        raise RuntimeError('Combat staging console did not open')

    with console, (out / 'stage.log').open('wb', buffering=0) as log:
        console.settimeout(.1)
        while not (out / 'frames.csv').exists():
            if process.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('No gameplay clock for combat staging')
            time.sleep(.1)
        start = time.monotonic()
        _at_tick(console, LOAD_TICK, '(core_load_name canyon.bin)')
        acknowledgement = b"loaded 'canyon.bin'"
        pending = b''
        camera_queued = False
        next_count = 14
        while process.poll() is None:
            elapsed = time.monotonic() - start
            if elapsed >= next_count:
                if not camera_queued:
                    raise RuntimeError('Combat checkpoint did not load at the scheduled tick')
                console.sendall(b'(ai_living_count c3_base_tier_2)\r\n')
                next_count += 12
            try:
                data = console.recv(8192)
            except socket.timeout:
                continue
            except ConnectionResetError:
                break
            if not data:
                break
            log.write(data)
            # Restoring the checkpoint also restores the script threads. Queue
            # the camera in the restored world, even if the reply spans reads.
            if not camera_queued:
                pending += data
                if acknowledgement in pending:
                    _at_tick(console, CAMERA_TICK, '(debug_camera_load)')
                    camera_queued = True
                pending = pending[-len(acknowledgement):]


def verify(out, engine_log, warmup, seconds, tick_range):
    console = (out / 'stage.log').read_text(errors='replace')
    counts = [int(value) for value in re.findall(r'(?m)^\s*(\d+)\s*$', console)]
    if "loaded 'canyon.bin'" not in console or not counts or len(counts) < (warmup + seconds - 14) // 12:
        raise ValueError('Missing checkpoint acknowledgement or combat telemetry')
    first, last = tick_range
    poses = [pose for tick, pose in re.findall(r'network test: tick (\d+) player 0:[^\n]*?local 0 camera \(([^)]+)\)', engine_log)
             if first <= int(tick) <= last]
    if counts[0] != 19 or len(poses) < seconds - 2 or any(p != '327.8 -649.3 74.5' for p in poses):
        raise ValueError('Combat squad or fixed exterior camera differs from the reference')
    return counts

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
