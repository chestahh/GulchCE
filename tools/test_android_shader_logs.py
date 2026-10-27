"""Exercise the shipped collector with a fake adb process, without a device."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'port/android/diagnostics/collect_shader_logs.py'
spec = importlib.util.spec_from_file_location('collect_shader_logs', SCRIPT)
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


@pytest.fixture
def fake_adb(tmp_path, monkeypatch):
    audit = tmp_path / 'commands.jsonl'
    monkeypatch.setenv('TEST_ADB_AUDIT', str(audit))
    script = tmp_path / 'adb.py'
    script.write_text(r'''
import json, os, sys, time
args = sys.argv[1:]
with open(os.environ['TEST_ADB_AUDIT'], 'a') as output:
    output.write(json.dumps(args)+'\n')
if args[:2] == ['-s', 'test-device']:
    args = args[2:]
if args == ['get-state']:
    print('device')
elif args[:2] == ['shell', 'getprop']:
    print('test-property')
elif args[:3] == ['shell', 'dumpsys', 'package']:
    print('versionName=test-build')
elif args[0] == 'logcat':
    if os.environ.get('TEST_ADB_DISCONNECT'):
        print('device disconnected', file=sys.stderr, flush=True)
        sys.exit(3)
    print('OpenGL ES 3.0 on test-GPU', flush=True)
    print('cannot compile the vertex shader:', flush=True)
    print('no precision specified for sampler3D', flush=True)
    print('uniform sampler3D plasma_occ_texture0;', flush=True)
    for i in range(500):
        print('shader source line '+str(i), flush=True)
    time.sleep(30)
else:
    sys.exit('unexpected command: '+repr(args))
''')
    return [sys.executable, str(script), '-s', 'test-device'], audit


def test_capture_preserves_shader_source_and_only_reads_device(fake_adb, tmp_path):
    adb, audit = fake_adb
    folder = tmp_path / 'capture'
    archive = collector.collect(adb, folder, 0.4)
    with zipfile.ZipFile(archive) as zipped:
        assert zipped.testzip() is None
        raw = zipped.read('shader-logcat.txt').decode()
        assert 'no precision specified for sampler3D' in raw
        assert 'shader source line 499' in raw
        assert len(raw) > 4096
        summary = zipped.read('summary.txt').decode()
        assert 'cannot compile' in summary and 'does not prove' in summary
        metadata = json.loads(zipped.read('device.json'))
        assert metadata['stop_reason'] == 'time limit'
        assert metadata['ro.product.model'] == 'test-property'
    calls = [json.loads(line) for line in audit.read_text().splitlines()]
    assert all(call[:2] == ['-s', 'test-device'] for call in calls)
    calls = [call[2:] for call in calls]
    assert calls == [['get-state'],
                     *[['shell', 'getprop', key] for key in collector.PROPERTIES],
                     ['shell', 'dumpsys', 'package', 'com.halo.decomp'],
                     ['logcat', '-v', 'threadtime', '-T', '1', 'halo:V', '*:S']]
    with pytest.raises(FileExistsError):
        collector.collect(adb, folder, 0.4)


def test_disconnect_retains_partial_diagnostics(fake_adb, tmp_path, monkeypatch):
    adb, _ = fake_adb
    monkeypatch.setenv('TEST_ADB_DISCONNECT', '1')
    folder = tmp_path / 'disconnected'
    with pytest.raises(RuntimeError, match='adb stopped'):
        collector.collect(adb, folder, 1)
    with zipfile.ZipFile(folder.with_suffix('.zip')) as zipped:
        assert 'device disconnected' in zipped.read('adb-errors.txt').decode()
        assert json.loads(zipped.read('device.json'))['logcat_exit_code'] == 3


@pytest.mark.parametrize('seconds', ['0', '601'])
def test_duration_is_bounded(seconds):
    result = subprocess.run([sys.executable, str(SCRIPT), '--seconds', seconds],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 2
    assert 'between 1 and 600' in result.stderr
