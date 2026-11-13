"""Check interpretation boundaries in the investigation's numerical report."""
import importlib.util
import json
from pathlib import Path

import pytest

SPEC = importlib.util.spec_from_file_location('measurements', Path(__file__).with_name('coop_m0_measurements.py'))
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)


def rows(path, values):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(''.join(','.join(map(str, row)) + '\n' for row in values))


def test_payload_identity_partial_seconds_and_repeated_ticks(tmp_path):
    manifest = {'binary_sha256': 'fixture', 'peers': {
        'host': {'environment': {'HALO_NET_ADDRESS': '127.0.0.236'}},
        'client': {'environment': {'HALO_NET_ADDRESS': '127.0.0.237'}}}}
    (tmp_path / 'manifest.json').write_text(json.dumps(manifest))
    (tmp_path / 'comparison.json').write_text('{}')
    rows(tmp_path / 'client/m0-network_objects.csv', [
        (0, 'snapshot_usable', 0, 0, 0, 0, 0, 0),
        (5.1, 'correction', 100, 1, 0, 3, 4, 0),
        (5.2, 'client_tick', 100, 1, 1, 0, 0, 0),
        (6, 'inventory_queue', 101, 8, 1, 0, 0, 0),
        (6.2, 'client_tick', 101, 1, 1, 0, 0, 0),
        # A checkpoint repeats ticks. Its correction must not overwrite the earlier sample.
        (7.1, 'correction', 100, 1, 0, 0, 0, 2),
        (7.2, 'client_tick', 100, 1, 1, 0, 0, 0),
        (8.5, 'client_tick', 101, 1, 1, 0, 0, 0)])
    peer = int(M.ipaddress.IPv4Address('127.0.0.237'))
    rows(tmp_path / 'host/m0-win32_net.csv', [
        (4.9, 'sendto', 1, 9999, peer, 2000, 0, 0),
        (5.1, 'sendto', 1, 100, peer, 2000, 0, 0),
        (5.2, 'sendto', 1, 9999, peer - 1, 2000, 0, 0),
        (8.4, 'send', 1, 50, peer, 2000, 0, 0)])
    result = M.measure(tmp_path)
    sent = result['payload']['host']['sent']
    assert sent['bytes'] == 150 and sent['udp_bytes'] == 100
    assert sent['bytes_per_second'] == pytest.approx(150 / 3.5)
    assert sent['complete_one_second_windows_bytes']['mean'] == pytest.approx(100 / 3)
    assert result['inventory_queue_peak'] == 8
    correction = result['client_state_write_max_per_observed_tick_world_units']
    assert correction['count'] == 4 and correction['mean'] == 1.75 and correction['max'] == 5


def test_incomplete_capture_is_not_silently_accepted(tmp_path):
    path = tmp_path / 'bad.csv'
    path.write_text('1,correction,4\n')
    with pytest.raises(ValueError, match='Incomplete diagnostic row'):
        M.read(path)


def test_empty_distribution_is_not_zero_measurement():
    assert M.stats([]) == {'count': 0}
