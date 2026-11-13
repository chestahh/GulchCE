"""Compare saved engine state with recovered state, independently of ready flags.

This is a bounded M0 oracle, not a certificate for unobserved engine fields.
QPC is shared by the two Windows processes. Logs must contain complete captures.
"""
import argparse
from collections import defaultdict
import csv
import io
import json
from pathlib import Path
import zipfile

from coop_m0_measurements import read


def evidence_rows(folder, name):
    path = folder / f'm0-{name}.csv'
    if path.exists():
        return read(path)
    archive = folder.parent / 'evidence.zip'
    if not archive.exists():
        return []
    with zipfile.ZipFile(archive) as stream:
        member = f'{folder.name}/{path.name}'
        if member not in stream.namelist():
            return []
        lines = csv.reader(io.StringIO(stream.read(member).decode('utf8')))
        result = []
        for row in lines:
            if len(row) != 8:
                raise ValueError(f'Incomplete diagnostic row in {member}: {row}')
            result.append({'time': float(row[0]), 'kind': row[1], 'v': list(map(float, row[2:]))})
        return result


def captures(folder, allow_truncated_tail=False):
    rows = evidence_rows(folder, 'network_coop')
    helpers = [r for name in ('hud_messaging', 'hud_nav_points', 'network_distributed')
               for r in evidence_rows(folder, name)
               if r['kind'] in ('objective', 'nav_player', 'nav', 'epoch')]
    result, current = [], None
    for row in rows:
        if row['kind'] == 'state_begin':
            if current is not None:
                raise ValueError('Nested/incomplete state capture')
            current = {'time': row['time'], 'tick': row['v'][0], 'rows': defaultdict(list)}
        if current is not None:
            current['rows'][row['kind']].append(row['v'][1:])
        if row['kind'] == 'state_end':
            if current is None:
                raise ValueError('State end without begin')
            for helper in helpers:
                if current['time'] <= helper['time'] <= row['time']:
                    current['rows'][helper['kind']].append(helper['v'][1:])
            result.append(current)
            current = None
    if current is not None:
        if not allow_truncated_tail:
            raise ValueError('Truncated final state capture')
        rows.append({'time': current['time'], 'kind': 'discarded_capture', 'v': [current['tick']]})
    return result, rows


def one(state, name):
    values = state['rows'].get(name, [])
    if len(values) != 1:
        raise ValueError(f'Expected one {name}, got {len(values)}')
    return values[0]


def summary(state, group):
    rows = state['rows']
    roster = sorted(rows.get('roster', []))
    units = {r[1] for r in roster if r[1] != -1}
    vitality = {int(r[0]): r[1:3] for r in rows.get('object_vitality', []) if r[0] in units}
    return {
        'time': state['time'], 'tick': state['tick'], 'bsp': one(state, 'state_begin')[0],
        'epoch': one(state, 'epoch')[:2], 'roster': roster,
        'hud_flags': one(state, 'hud_flags'), 'objective': one(state, 'objective')[0],
        'nav': sorted([r[1:] for r in rows.get('nav', [])]),
        'nav_owners': rows.get('nav_player', []),
        'device_group': [r for r in rows.get('device_group', []) if r[0] == group],
        'player_maxima': vitality,
        'player_inventory': sorted(r[:3] for r in rows.get('inventory', []) if r[0] in units),
        'player_seats': sorted(r[:2] for r in rows.get('unit', []) if r[0] in units),
        'presentation': one(state, 'presentation'), 'timer': one(state, 'timer'),
        'allegiance': rows.get('allegiance', []),
        'host_caches': {name: rows.get(name, []) for name in
                        ('vitality_cache', 'hud_cache', 'nav_cache', 'attachment_cache', 'loop_cache')},
        'object_count': len(rows.get('object', [])),
    }


def compare_fields(expected, actual, fields):
    return {key: {'expected': expected[key], 'actual': actual[key]}
            for key in fields if expected[key] != actual[key]}


def relationships(state):
    """Actual references must resolve; NONE is an explicit empty relationship."""
    rows = state['rows']
    objects = {r[0]: r for r in rows.get('object', [])}
    failures = []
    for row in rows.get('roster', []):
        if row[1] != -1 and row[1] not in objects:
            failures.append(['roster_unit', row[:2]])
    for row in objects.values():
        if row[4] != -1 and row[4] not in objects:
            failures.append(['object_parent', row])
    for row in rows.get('inventory', []):
        if row[0] not in objects or (row[2] != -1 and row[2] not in objects):
            failures.append(['inventory_reference', row[:3]])
    return failures


def stable_objects(before, after, client):
    """Compare only ordinary replicated identities stable across host samples.

    A snapshot sampled on different ticks cannot establish movement equality.
    Exclude locally constructed scenery/devices/effects; those need name mapping.
    """
    def indexed(state):
        return {r[0]: r for r in state['rows'].get('object', []) if r[2] in (0, 1, 2, 3)}
    a, b, c = indexed(before), indexed(after), indexed(client)
    stable = {key: row for key, row in a.items() if b.get(key) == row}
    failures = [{'datum': int(key), 'expected': row, 'actual': c.get(key)}
                for key, row in stable.items() if c.get(key) != row]
    return {'stable_host_objects': len(stable), 'mismatch_count': len(failures), 'examples': failures[:12],
            'limits': 'identity/tag/type/name/parent only; no absence proof outside stable host set'}


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    states, rows = {}, {}
    controlled_stop = manifest.get('cleanup_errors') == [] and all(
        'early_exit' in p and p['early_exit'] is None for p in manifest['peers'].values())
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled_stop)
    phases = [r for r in rows['host'] if r['kind'] == 'fixture_phase']
    saves = [r for r in rows['host'] if r['kind'] == 'checkpoint_saved' and r['v'][1] == 1]
    result = {'case': folder.name, 'early_exits': {r: p.get('early_exit') for r, p in manifest['peers'].items()},
              'phases': phases, 'save_callbacks': saves,
              'captures': {r: len(s) for r, s in states.items()}, 'complete_sequence': False}
    result['discarded_final_captures'] = {role: [r for r in rows[role] if r['kind'] == 'discarded_capture']
                                        for role in rows}
    if [r['v'][1] for r in phases] != list(range(7)):
        result['reason'] = 'Missing fixture phases; not recovery evidence'
        return result
    saved = [r for r in saves if phases[1]['time'] <= r['time'] < phases[2]['time']]
    if len(saved) != 1:
        result['reason'] = 'Expected exactly one completed fixture save'
        return result
    # An additional checkpoint after B would invalidate the A recovery contract.
    if any(phases[2]['time'] <= r['time'] <= phases[6]['time'] for r in saves):
        result['reason'] = 'Checkpoint overwritten after mutation'
        return result
    group = phases[0]['v'][4]
    selected = {'saved': max((s for s in states['host'] if phases[0]['time'] <= s['time'] < saved[0]['time']), key=lambda s: s['time']),
                'changed': max((s for s in states['host'] if phases[2]['time'] < s['time'] < phases[3]['time']), key=lambda s: s['time'])}
    for label, phase in (('restored', 4), ('restored_again', 6)):
        selected[label] = min((s for s in states['host'] if s['time'] >= phases[phase]['time']), key=lambda s: s['time'])
    result['samples'] = {}
    controlled = ('hud_flags', 'objective', 'nav', 'device_group', 'player_maxima')
    durable = (*controlled, 'roster', 'player_inventory', 'player_seats', 'bsp')
    for label, host in selected.items():
        client = min(states['client'], key=lambda s: abs(s['time'] - host['time']))
        delta = abs(client['time'] - host['time'])
        pair = {r: summary(s, group) for r, s in (('host', host), ('client', client))}
        pair['sample_skew_seconds'] = delta
        pair['peer_disagreement'] = compare_fields(pair['host'], pair['client'], durable)
        pair['references'] = {r: relationships(s) for r, s in (('host', host), ('client', client))}
        later = next((s for s in states['host'] if s['time'] > host['time'] + .5), None)
        if later:
            pair['stable_objects'] = stable_objects(host, later, client)
        result['samples'][label] = pair
    base = result['samples']['saved']['host']
    result['mutations_observed'] = {k: base[k] != result['samples']['changed']['host'][k] for k in controlled}
    result['epoch_sequence_valid'] = all(
        result['samples'][label]['host']['epoch'][0] == base['epoch'][0] + offset
        and result['samples'][label]['client']['epoch'] == [base['epoch'][0] + offset, 1]
        for label, offset in (('restored', 1), ('restored_again', 2)))
    result['restoration_failures'] = {label: {role: compare_fields(base, result['samples'][label][role], durable)
        for role in ('host', 'client')} for label in ('restored', 'restored_again')}
    result['recovery_windows'] = {}
    for label, start, end in (('first', phases[3]['time'] + 2, phases[5]['time']),
                              ('second', phases[5]['time'] + 2, float('inf'))):
        result['recovery_windows'][label] = {}
        for role in ('host', 'client'):
            window = [s for s in states[role] if start <= s['time'] < end]
            values = [summary(s, group) for s in window]
            result['recovery_windows'][label][role] = {
                'samples': len(values), 'duration_seconds': window[-1]['time'] - window[0]['time'] if window else 0,
                'client_ready_samples': sum(v['epoch'][1] == 1 for v in values) if role == 'client' else None,
                'epochs': sorted({v['epoch'][0] for v in values}),
                'wrong_saved_value_samples': {key: sum(v[key] != base[key] for v in values) for key in durable}}
    result['complete_sequence'] = True
    result['sample_skew_within_1_1_seconds'] = all(p['sample_skew_seconds'] <= 1.1 for p in result['samples'].values())
    result['objective_fault_rows'] = sum(r['kind'] == 'fault_drop_objective' for r in rows['client'])
    result['limits'] = 'Controlled field restoration and sampled relationships only. Not full-state, rendering, audio, AI or platform acceptance.'
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'recovery-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result.get(k) for k in ('case', 'complete_sequence', 'reason', 'mutations_observed', 'restoration_failures')}))
