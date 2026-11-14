"""Reduce the private sustained-recovery experiment without hiding early failures."""
import argparse
import json
from pathlib import Path

from coop_m0_identity_measurements import state_values
from coop_m0_recovery_measurements import captures, evidence_rows, one
from coop_m0_timing_measurements import save_window


def target_values(state, units, handle):
    values = state_values(state, units, handle, [])
    # This fixture does not enumerate explicit post-save replacement objects.
    # An empty list would otherwise look like a passing absence assertion.
    del values['post_save_presence']
    return values


def gate_observations(rows):
    result, pending = [], None
    for row in rows:
        if row['kind'] == 'stress_gate':
            if pending is not None:
                raise ValueError('Incomplete earlier stress gate')
            pending = {'time': row['time'], 'tick': row['v'][0], 'epoch': row['v'][1],
                'ready': bool(row['v'][2]), 'synchronized': bool(row['v'][3]),
                'queued': row['v'][4], 'retry_ticks': row['v'][5], 'targets': []}
        elif row['kind'] == 'stress_inventory':
            if pending is None or row['v'][0] != pending['tick'] or row['v'][1] != len(pending['targets']):
                raise ValueError('Mismatched stress inventory')
            pending['targets'].append({'handle': row['v'][2], 'exists': bool(row['v'][3]),
                'inventory_correct': row['v'][4] == row['v'][2], 'failed_time': row['v'][5]})
            if len(pending['targets']) == 2:
                result.append(pending)
                pending = None
    return result, pending


def summarize_gate(values, start):
    def complete(v):
        return v['queued'] == 0 and all(t['exists'] and t['inventory_correct'] for t in v['targets'])
    first = next((v for v in values if complete(v)), None)
    incomplete = [v for v in values if v['ready'] and not complete(v)]
    return {'samples': len(values), 'ready_incomplete': len(incomplete),
        'both_flags_ready_incomplete': sum(v['synchronized'] for v in incomplete),
        'ready_unsynchronized': sum(v['ready'] and not v['synchronized'] for v in values),
        'max_queued': max((v['queued'] for v in values), default=0),
        'max_retry_ticks': max((v['retry_ticks'] for v in values), default=0),
        'first_complete_seconds': first['time'] - start if first else None,
        'last_ready_incomplete_seconds': incomplete[-1]['time'] - start if incomplete else None,
        'incorrect_after_first_complete': sum(not complete(v) for v in values if first and v['time'] > first['time'])}


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    controlled = not manifest['cleanup_errors'] and all(p['early_exit'] is None for p in manifest['peers'].values())
    states, rows = {}, {}
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled)
    events = evidence_rows(folder / 'client', 'network_objects')
    gates, trailing = gate_observations(events)
    kinds = lambda name: [r for r in rows['host'] if r['kind'] == name]
    phases, reverts, targets = map(kinds, ('stress_phase', 'identity_revert', 'stress_target'))
    result = {'case': folder.name, 'fixture_complete': False, 'controlled_stop': controlled,
        'early_exits': {role: p['early_exit'] for role, p in manifest['peers'].items()},
        'phases': phases, 'reverts': reverts, 'targets': targets, 'trailing_gate': trailing,
        'checkpoint_pins': kinds('checkpoint_pin'),
        'discarded_final_captures': {role: [r for r in rows[role] if r['kind'] == 'discarded_capture'] for role in rows},
        'events': [e for e in events if e['kind'] in ('create_failed', 'retry_request', 'reset_before',
            'reset_after', 'dependency_hold', 'dependency_release', 'dependency_cancel')],
        'pool': evidence_rows(folder / 'client', 'objects'),
        'packets': [e for e in evidence_rows(folder / 'client', 'network_distributed') if e['kind'].startswith('old_')]}
    if [r['v'][1] for r in phases] != list(range(10)) or len(reverts) != 4 or len(targets) != 2:
        result['reason'] = 'Incomplete phase/revert/target sequence'
        return result
    if trailing and not controlled:
        result['reason'] = 'Uncontrolled split gate'
        return result
    try:
        saved, later = save_window(kinds('checkpoint_saved'), phases[1]['time'], phases[2]['time'], reverts[-1]['time'])
    except ValueError as error:
        result['reason'] = str(error)
        return result
    units = [r['v'][2] for r in targets]
    handles = [r['v'][3] for r in targets]
    result.update(save=saved, later_saves=later, mutations={}, windows={})
    selected = max((s for s in states['host'] if phases[0]['time'] < s['time'] < saved['time']), key=lambda s: s['time'])
    expected = [target_values(selected, units, handle) for handle in handles]
    result['saved_values'] = expected
    result['targets_added'] = all(r['v'][5] == 1 for r in targets)
    client_saved = min(states['client'], key=lambda s: abs(s['time'] - selected['time']))
    result['saved_peer_skew_seconds'] = abs(client_saved['time'] - selected['time'])
    result['saved_peer_agreement'] = all(target_values(client_saved, units, h) == value for h, value in zip(handles, expected))
    result['saved_targets_qualified'] = all(
        len(value['slot_contents']) == 1 and value['slot_contents'][0][:3] == [handles[i], targets[i]['v'][4], 2]
        and value['all_tracked_inventory_references'] == [[units[i], 0, handles[i]]]
        and [handles[i], 0, 20 + 4 * i, 20 + 4 * i] in value['primary_magazines']
        for i, value in enumerate(expected))
    for begin, end in ((2, 3), (4, 5), (6, 7)):
        # Require both peers to have actually lost both saved objects before
        # the next restore; a host-only mutation is not a qualified fixture.
        result['mutations'][str(begin)] = {}
        for role in states:
            candidates = [s for s in states[role] if phases[begin]['time'] < s['time'] < phases[end]['time']]
            result['mutations'][str(begin)][role] = any(
                all(not any(r[0] == h for r in s['rows'].get('object', [])) for h in handles) for s in candidates)
    for offset, revert in enumerate(reverts):
        epoch = offset + 2
        end = (phases[4]['time'], phases[6]['time'], reverts[3]['time'],
            min((r['time'] for r in later), default=float('inf')))[offset]
        samples = [v for v in gates if v['epoch'] == epoch and revert['time'] <= v['time'] < end]
        window = {'gate': summarize_gate(samples, revert['time']), 'peers': {}}
        for role in states:
            captures_in_window = [s for s in states[role] if one(s, 'epoch')[0] == epoch and revert['time'] <= s['time'] < end]
            tail = [s for s in captures_in_window if s['time'] >= captures_in_window[-1]['time'] - 5] if captures_in_window else []
            wrong = {key: 0 for key in expected[0]}
            for state in tail:
                for handle, baseline in zip(handles, expected):
                    actual = target_values(state, units, handle)
                    for key in wrong:
                        wrong[key] += actual[key] != baseline[key]
            window['peers'][role] = {'samples': len(captures_in_window), 'tail_samples': len(tail),
                'tail_wrong_fields': wrong}
        result['windows'][str(epoch)] = window
    result['fixture_complete'] = True
    result['observations_complete'] = all(w['gate']['samples'] >= 100 and all(p['tail_samples'] >= 4 for p in w['peers'].values()) for w in result['windows'].values())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'stress-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
