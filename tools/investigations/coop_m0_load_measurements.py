"""Reduce real-engine M0 load observations without treating readiness as recovery.

Run before archiving CSVs. Rows retain QPC timestamps for cross-peer comparisons.
Private memory is process commit, not object-pool occupancy. Tick timings exclude
rendering; host object networking is a separately measured engine call.
"""
import argparse
import json
from pathlib import Path

from coop_m0_measurements import measure, read, stats


def reduce(folder):
    result = measure(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    result['early_exits'] = {role: entry['early_exit'] for role, entry in manifest['peers'].items()}
    result['requested'] = int(manifest['peers']['host']['environment']['M0_LOAD_OBJECTS'])
    result['template_kind'] = 'biped' if 'M0_LOAD_BIPEDS' in manifest['peers']['host']['environment'] else 'weapon'
    objects = {role: read(folder / role / 'm0-network_objects.csv') for role in ('host', 'client')}
    kinds = ('load_before', 'load_created', 'load_after', 'load_invalid', 'load_no_template',
             'snapshot_counts', 'received_counts', 'snapshot_begin', 'snapshot_built', 'snapshot_usable')
    result['boundaries'] = {role: [row for row in rows if row['kind'] in kinds] for role, rows in objects.items()}
    result['pool_observations'] = {role: [row for row in rows if row['kind'] == 'pool_counts'] for role, rows in objects.items()}
    host_counts = next((r for r in objects['host'] if r['kind'] == 'snapshot_counts'), None)
    begin = next((r for r in objects['host'] if r['kind'] == 'snapshot_begin'), None)
    received = next((r for r in objects['client'] if r['kind'] == 'received_counts'), None)
    if host_counts and begin and received:
        # These fixtures never request a revert or a second world. Cached replies
        # can arrive before the first snapshot is applied; do not misattribute
        # the first ready marker to the latest cached host reply.
        result['initial_snapshot_observation'] = {
            'host_counts': host_counts, 'client_counts': received,
            'count_and_identity_tag_fingerprint_agree': host_counts['v'][1:3] == received['v'][1:3]
                and host_counts['v'][5] == received['v'][5],
            'begin_to_received_counts_ms': (received['time'] - begin['time']) * 1000}
    observations = json.loads((folder / 'observations.json').read_text())
    result['memory_whole_session'] = {role: {key: stats([r[role][key] for r in observations if key in r[role]])
        for key in ('working_bytes', 'private_bytes')} for role in objects}
    if result.get('measured'):
        start, end = result['window_qpc_seconds']
        result['steady_tick_ms'] = {role: stats([r['v'][1] for r in read(folder / role / 'm0-game.csv')
            if r['kind'] == 'tick_ms' and start <= r['time'] <= end]) for role in objects}
        result['steady_network_tick_ms'] = {role: stats([r['v'][1] for r in read(folder / role / 'm0-network_distributed.csv')
            if r['kind'] == 'network_tick_ms' and start <= r['time'] <= end]) for role in objects}
        result['steady_host_object_network_ms'] = stats([r['v'][1] for r in objects['host']
            if r['kind'] == 'host_objects_ms' and start <= r['time'] <= end])
        result['steady_live_counts'] = [r for r in objects['host'] if r['kind'] == 'live_counts'
                                       and start <= r['time'] <= end]
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'load-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result.get(k) for k in ('measured', 'requested', 'early_exits', 'snapshots',
                                               'steady_tick_ms', 'steady_host_object_network_ms')}))
