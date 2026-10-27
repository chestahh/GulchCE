#!/usr/bin/env python3
"""Read GulchCE shader/device logs over adb without changing app data or settings."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import subprocess
import time
import zipfile

PACKAGE = 'com.halo.decomp'
PROPERTIES = ('ro.product.manufacturer', 'ro.product.model', 'ro.board.platform',
              'ro.build.version.release', 'ro.build.version.sdk', 'ro.opengles.version')
MAX_LOG_BYTES = 32 * 1024 * 1024


def query(adb, *args):
    result = subprocess.run([*adb, *args], capture_output=True, text=True,
                            errors='replace', timeout=15)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or 'adb command failed')
    return result.stdout.strip()


def collect(adb, output, seconds):
    if query(adb, 'get-state') != 'device':
        raise RuntimeError('Authorize USB debugging on the phone, then retry.')
    output.mkdir(parents=True, exist_ok=False)
    metadata = {'started_utc': datetime.now(timezone.utc).isoformat(),
                'duration_limit_seconds': seconds, 'package': PACKAGE}
    for key in PROPERTIES:
        metadata[key] = query(adb, 'shell', 'getprop', key)
    (output / 'app-version.txt').write_text(query(adb, 'shell', 'dumpsys', 'package', PACKAGE), encoding='utf-8')
    revision = Path(__file__).with_name('build-revision.txt')
    if revision.exists():
        shutil.copy2(revision, output / revision.name)
    log = output / 'shader-logcat.txt'
    errors = output / 'adb-errors.txt'
    command = [*adb, 'logcat', '-v', 'threadtime', '-T', '1', 'halo:V', '*:S']
    print('Capture started. Close and reopen GulchCE now, then reproduce the issue.', flush=True)
    print(f'Recording for up to {seconds:g} seconds. Ctrl+C finishes early.', flush=True)
    start = time.monotonic()
    with log.open('wb') as raw, errors.open('wb') as err:
        process = subprocess.Popen(command, stdout=raw, stderr=err,
                                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        reason = 'time limit'
        try:
            while time.monotonic() - start < seconds:
                if process.poll() is not None:
                    reason = 'adb exited early'
                    break
                if log.stat().st_size >= MAX_LOG_BYTES:
                    reason = 'log size limit'
                    break
                time.sleep(0.1)
        except KeyboardInterrupt:
            reason = 'stopped by tester'
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    metadata['stop_reason'] = reason
    metadata['captured_seconds'] = round(time.monotonic() - start, 2)
    metadata['logcat_exit_code'] = process.returncode
    (output / 'device.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    # Keep complete shader lines in the raw capture, not just error matches.
    text = log.read_text(encoding='utf-8', errors='replace')
    markers = ('cannot compile', 'cannot link', 'OpenGL ', 'GL error', 'GPU:', 'guest abort', 'fatal')
    summary = [line for line in text.splitlines() if any(marker in line for marker in markers)]
    (output / 'summary.txt').write_text('\n'.join(summary) + '\n\n'
        'No matching errors does not prove correct rendering. See shader-logcat.txt for full shader source.\n',
        encoding='utf-8')
    (output / 'reproduction.txt').write_text(
        'Map name and family (Xbox/CE/MCC):\nWeapon or object affected:\n'
        'Graphics settings and FPS cap:\nWhat you saw and approximate capture time:\n'
        'Did the same map work before updating?\n', encoding='utf-8')
    archive = output.with_name(output.name + '.zip')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as package:
        for path in sorted(output.iterdir()):
            package.write(path, path.name)
    print(f'Logs saved to {output}\nShare {archive} with your reproduction details.', flush=True)
    if reason == 'adb exited early':
        raise RuntimeError(f'adb stopped before the capture ended; inspect {errors}. Partial logs were retained.')
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb', help='Path to Android platform-tools adb')
    parser.add_argument('--serial', help='Device serial if more than one device is connected')
    parser.add_argument('--seconds', type=float, default=120, help='Capture duration (1 to 600; default 120)')
    parser.add_argument('--output', type=Path, default=Path('shader-logs-' + datetime.now().strftime('%Y%m%d-%H%M%S')))
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error('--seconds must be between 1 and 600')
    adb = [args.adb] + (['-s', args.serial] if args.serial else [])
    try:
        collect(adb, args.output.resolve(), args.seconds)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'{error}\nInstall Android platform-tools and authorize the selected USB device.\n')


if __name__ == '__main__':
    main()
