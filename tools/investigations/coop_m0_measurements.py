"""Reduce M0 diagnostic CSVs; preserve distinctions between payload and wire bytes.

Only successful socket payload bytes addressed to the other loopback peer count.
TCP/IP headers, ACKs, retransmissions and public-internet overhead are excluded.
QPC timestamps share a clock on this Windows host. Network-write magnitudes are
measured displacement, not the once-per-second player comparison's disagreement.
"""
import argparse
import collections
import csv
import ipaddress
import json
import math
from pathlib import Path


def read(path):
    if not path.exists():
        return []
    rows = []
    with path.open(newline='') as stream:
        for row in csv.reader(stream):
            if len(row) != 8:
                raise ValueError(f'Incomplete diagnostic row in {path}: {row}')
            rows.append({'time': float(row[0]), 'kind': row[1], 'v': list(map(float, row[2:]))})
    return rows


def stats(values):
    if not values:
        return {'count': 0}
    values = sorted(values)
    return {'count': len(values), 'mean': sum(values) / len(values), 'max': max(values),
            **{f'p{p}': values[math.ceil(p / 100 * len(values)) - 1] for p in (50, 95, 99)}}


def measure(folder):
    manifest = json.loads((folder / 'manifest.json').read_text())
    objects = {role: read(folder / role / 'm0-network_objects.csv') for role in ('host', 'client')}
    sockets = {role: read(folder / role / 'm0-win32_net.csv') for role in ('host', 'client')}
    usable = [r for r in objects['client'] if r['kind'] == 'snapshot_usable']
    ticks = [r for r in objects['client'] if r['kind'] == 'client_tick']
    if not usable or not ticks:
        return {'measured': False, 'reason': 'No observed client synchronization/tick interval'}
    start, end = usable[0]['time'] + 5, ticks[-1]['time']
    seconds = end - start
    if seconds < 1:
        return {'measured': False, 'reason': 'Insufficient post-synchronization observation'}
    result = {'measured': True, 'window_qpc_seconds': [start, end], 'seconds': seconds,
              'comparison': json.loads((folder / 'comparison.json').read_text()), 'payload': {},
              'binary_sha256': manifest['binary_sha256'], 'peer_binary_sha256': {
                  r: p.get('binary_sha256', manifest['binary_sha256']) for r, p in manifest['peers'].items()}}
    for role, other in (('host', 'client'), ('client', 'host')):
        address = int(ipaddress.IPv4Address(manifest['peers'][other]['environment']['HALO_NET_ADDRESS']))
        samples = [r for r in sockets[role] if start <= r['time'] <= end and r['v'][2] == address]
        result['payload'][role] = {}
        for direction, kinds in (('sent', ('send', 'sendto')), ('received', ('recv', 'recvfrom'))):
            selected = [r for r in samples if r['kind'] in kinds]
            complete_seconds = int(seconds)
            buckets = [0] * complete_seconds
            for r in selected:
                second = int(r['time'] - start)
                if second < complete_seconds:
                    buckets[second] += int(r['v'][1])
            result['payload'][role][direction] = {
                'bytes': sum(int(r['v'][1]) for r in selected),
                'bytes_per_second': sum(r['v'][1] for r in selected) / seconds,
                'complete_one_second_windows_bytes': stats(buckets),
                'successful_calls': len(selected),
                'udp_bytes': sum(int(r['v'][1]) for r in selected if r['kind'] in ('sendto', 'recvfrom'))}
    by_kind = collections.defaultdict(list)
    by_tick = collections.defaultdict(float)
    generation, previous_tick = 0, None
    observed_ticks = []
    for r in objects['client']:
        if r['kind'] not in ('client_tick', 'correction'):
            continue
        tick = int(r['v'][0])
        if previous_tick is not None and tick < previous_tick:
            generation += 1
        previous_tick = tick
        key = (generation, tick)
        if r['kind'] == 'client_tick' and start <= r['time'] <= end:
            observed_ticks.append(key)
        if r['kind'] == 'correction' and start <= r['time'] <= end:
            magnitude = math.sqrt(sum(v * v for v in r['v'][3:6]))
            by_kind[int(r['v'][2])].append(magnitude)
            if int(r['v'][2]) == 0:
                by_tick[key] = max(by_tick[key], magnitude)
    result['client_network_writes_world_units_by_kind'] = {
        kind: {'all': stats(values), 'nonzero': stats([v for v in values if v > 0])}
        for kind, values in by_kind.items()}
    result['client_state_write_max_per_observed_tick_world_units'] = stats([by_tick[t] for t in observed_ticks])
    result['inventory_queue_peak'] = max([r['v'][3] for r in ticks] +
        [r['v'][1] for r in objects['client'] if r['kind'] == 'inventory_queue'])
    result['create_failure_counter_max'] = max(r['v'][4] for r in ticks)
    events = read(folder / 'host/m0-network_coop.csv')
    result['event_queue_peak'] = max([r['v'][1] for r in events if r['kind'] in ('event_queue', 'event_overflow')], default=0)
    result['event_overflow_observations'] = sum(r['kind'] == 'event_overflow' for r in events)
    messages = read(folder / 'host/m0-network_distributed.csv')
    begins = [r for r in objects['host'] if r['kind'] == 'snapshot_begin']
    result['snapshots'] = []
    for i, begin in enumerate(begins):
        limit = begins[i + 1]['time'] if i + 1 < len(begins) else float('inf')
        finished = next((r for r in objects['host'] if begin['time'] <= r['time'] < limit
                         and r['kind'] in ('snapshot_built', 'snapshot_cached')), None)
        if not finished:
            continue
        marker = next((r for r in messages if r['kind'] == 'reliable_enqueue'
                       and finished['time'] <= r['time'] < limit and r['v'][2] == 8), None)
        # Cached replies log after sending their marker; they are reported separately.
        if finished['kind'] == 'snapshot_cached':
            result['snapshots'].append({'host_tick': begin['v'][0], 'cached_reply': True})
            continue
        last = marker['time'] if marker else finished['time']
        offered = [r for r in messages if r['kind'] == 'reliable_enqueue'
                   and begin['time'] <= r['time'] <= last and r['v'][1] == begin['v'][1]]
        ready = next((r for r in usable if last <= r['time'] < limit), None)
        result['snapshots'].append({'host_tick': begin['v'][0],
            'host_construction_ms': (finished['time'] - begin['time']) * 1000,
            'offered_message_bytes': sum(int(r['v'][3]) for r in offered),
            'serialized_object_entries': sum(int(r['v'][5]) for r in offered if r['v'][2] == 5),
            'host_begin_to_client_usable_ms': (ready['time'] - begin['time']) * 1000 if ready else None,
            'client_pending_inventories_at_usable': ready['v'][2] if ready else None})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sessions', type=Path, default=Path('build/m0/sessions'))
    parser.add_argument('--output', type=Path, default=Path('docs/coop_m0_measurements.json'))
    args = parser.parse_args()
    # First pilot used nine significant digits, insufficient to distinguish peer IPs.
    folders = sorted(p for p in args.sessions.glob('m0b-*trace') if (p / 'comparison.json').exists())
    result = {p.name: measure(p) for p in folders}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({name: {'seconds': v.get('seconds'), 'snapshots': v.get('snapshots')}
                      for name, v in result.items()}))


if __name__ == '__main__':
    main()
