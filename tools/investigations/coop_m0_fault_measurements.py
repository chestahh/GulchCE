"""Characterize cross-type restoration and injected reconstruction failures.

Ready flags do not define success. Per-tick gate observations and independent
saved-state comparisons describe both temporary failures and eventual repair.
"""
import argparse
import json
from pathlib import Path

from coop_m0_identity_measurements import reused_slot, state_values
from coop_m0_recovery_measurements import captures, evidence_rows, one, relationships
from coop_m0_timing_measurements import save_window


def cross_checks(a, b, units, original, replacement, definitions):
    return {
        'same_slot_new_generation': reused_slot(original, replacement),
        'saved_weapon': len(a['slot_contents']) == 1 and a['slot_contents'][0][:3] == [original, definitions[0], 2],
        'replacement_biped': len(b['slot_contents']) == 1 and b['slot_contents'][0][:3] == [replacement, definitions[1], 0],
        'saved_inventory': a['all_tracked_inventory_references'] == [[units[0], 0, original]],
        'changed_empty_inventory': bool(b['inventories']) and all(r[2] == -1 for r in b['inventories']),
        'saved_primary_rounds': [original, 0, 20, 20] in a['primary_magazines'],
        'post_save_absence_at_save': not any(a['post_save_presence']),
    }


def gate_observations(rows):
    pending, result = None, []
    for row in rows:
        if row['kind'] == 'fault_state':
            if pending is not None:
                raise ValueError('Unpaired fault state observation')
            pending = row
        elif row['kind'] == 'fault_inventory':
            if pending is None or pending['v'][0] != row['v'][0]:
                raise ValueError('Missing/mismatched fault inventory observation')
            tick, epoch, ready, synchronized, queued, exists = pending['v']
            result.append({'time': pending['time'], 'tick': tick, 'epoch': epoch, 'ready': bool(ready),
                'objects_synchronized': bool(synchronized), 'pending_inventories': queued,
                'target_exists': bool(exists), 'inventory_correct': row['v'][2] == row['v'][3],
                'failed_time': row['v'][4], 'retry_interval': row['v'][5]})
            pending = None
    # A controlled process stop may split the final pair; no earlier pair is discarded.
    return result, pending


def gate_summary(observations, start, end, epoch):
    values = [v for v in observations if start <= v['time'] < end and v['epoch'] == epoch]
    correct = lambda v: v['target_exists'] and v['inventory_correct'] and v['pending_inventories'] == 0
    missing = [v for v in values if v['ready'] and not correct(v)]
    repaired = next((v for v in values if correct(v)), None)
    return {'samples': len(values), 'ready_samples': sum(v['ready'] for v in values),
        'ready_with_incomplete_state': len(missing),
        'ready_missing_target': sum(v['ready'] and not v['target_exists'] for v in values),
        'ready_wrong_inventory': sum(v['ready'] and not v['inventory_correct'] for v in values),
        'ready_while_objects_unsynchronized': sum(v['ready'] and not v['objects_synchronized'] for v in values),
        'maximum_pending_inventories': max((v['pending_inventories'] for v in values), default=0),
        'first_ready_incomplete_after_revert_seconds': missing[0]['time'] - start if missing else None,
        'last_ready_incomplete_after_revert_seconds': missing[-1]['time'] - start if missing else None,
        'first_complete_after_revert_seconds': repaired['time'] - start if repaired else None,
        'incorrect_samples_after_first_complete': sum(not correct(v) for v in values if repaired and v['time'] > repaired['time'])}


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    controlled = manifest.get('cleanup_errors') == [] and all(p.get('early_exit', 'missing') is None for p in manifest['peers'].values())
    states, rows = {}, {}
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled)
    kinds = lambda kind: [r for r in rows['host'] if r['kind'] == kind]
    phases, reverts, context, allocations, additions, replacements = map(kinds, (
        'fixture_phase', 'identity_revert', 'identity_context', 'identity_new', 'identity_add', 'cross_replacement'))
    fault_rows = evidence_rows(folder / 'client', 'network_objects')
    observations, trailing = gate_observations(fault_rows)
    result = {'case': folder.name, 'complete_sequence': False,
        'early_exits': {r: p['early_exit'] for r, p in manifest['peers'].items()},
        'discarded_final_captures': {r: [v for v in rows[r] if v['kind'] == 'discarded_capture'] for r in rows},
        'trailing_gate_half_pair': trailing, 'allocations': allocations, 'additions': additions, 'reverts': reverts,
        'fault_events': [r for r in fault_rows if r['kind'] in (
            'fault_target', 'allocation_refused', 'create_failed', 'dependency_held', 'dependency_released', 'dependency_cancelled')]}
    if trailing and not controlled:
        result['reason'] = 'Uncontrolled termination split a gate observation'
        return result
    if [p['v'][1] for p in phases] != list(range(7)) or len(context) != 1 or len(reverts) != 2 or len(replacements) != 2:
        result['reason'] = 'Incomplete fixture sequence, context, replacements or reverts'
        return result
    try:
        saved, later_saves = save_window(kinds('checkpoint_saved'), phases[1]['time'], phases[2]['time'], reverts[-1]['time'])
    except ValueError as error:
        result['reason'] = str(error)
        return result
    if len(additions) != 1 or additions[0]['v'][4] != 1:
        result['reason'] = 'Saved inventory addition failed'
        return result
    original = additions[0]['v'][3]
    post_save = list(dict.fromkeys(r['v'][2] for r in allocations if r['v'][1] > 0))
    units, definitions = context[0]['v'][1:3], context[0]['v'][3:5]
    if not post_save or -1 in post_save:
        result['reason'] = 'Missing post-save allocation'
        return result
    selected = {'saved': max((s for s in states['host'] if phases[0]['time'] <= s['time'] < saved['time']), key=lambda s: s['time'])}
    for label, begin, end in (('changed', 2, 3), ('restored', 3, 4), ('changed_again', 4, 5)):
        selected[label] = max((s for s in states['host'] if phases[begin]['time'] < s['time'] < phases[end]['time']), key=lambda s: s['time'])
    selected['restored_again'] = min((s for s in states['host'] if s['time'] >= phases[6]['time']), key=lambda s: s['time'])
    samples = {}
    for label, host in selected.items():
        client = min(states['client'], key=lambda s: abs(s['time'] - host['time']))
        samples[label] = {'skew_seconds': abs(host['time'] - client['time'])}
        for role, state in (('host', host), ('client', client)):
            samples[label][role] = {'tick': state['tick'], 'values': state_values(state, units, original, post_save),
                'epoch': one(state, 'epoch')[:2], 'reference_failures': relationships(state)}
    a = samples['saved']['host']['values']
    result.update(original=original, slot=int(original) & 0xffff, units=units, definitions=definitions,
        replacements=replacements, save=saved, later_saves=later_saves, samples=samples, mutations_observed={}, recovery_windows={})
    for label, replacement in zip(('changed', 'changed_again'), replacements):
        result['mutations_observed'][label] = {role: cross_checks(samples['saved'][role]['values'], samples[label][role]['values'],
            units, original, replacement['v'][3], definitions) for role in ('host', 'client')}
    base_epoch = samples['saved']['host']['epoch'][0]
    result['epoch_sequence_valid'] = all(samples[label]['host']['epoch'][0] == base_epoch + offset
        and samples[label]['client']['epoch'] == [base_epoch + offset, 1]
        for label, offset in (('restored', 1), ('restored_again', 2)))
    for index, revert in enumerate(reverts):
        end = phases[4]['time'] if index == 0 else min((s['time'] for s in later_saves), default=float('inf'))
        window_result = {'ended_at_later_save': index == 1 and bool(later_saves),
            'client_gate': gate_summary(observations, revert['time'], end, base_epoch + index + 1), 'peers': {}}
        for role in ('host', 'client'):
            window = [s for s in states[role] if revert['time'] <= s['time'] < end and one(s, 'epoch')[0] == base_epoch + index + 1]
            values = [state_values(s, units, original, post_save) for s in window]
            tail = [s for s in window if s['time'] >= window[-1]['time'] - 5] if window else []
            tail_values = [state_values(s, units, original, post_save) for s in tail]
            window_result['peers'][role] = {'samples': len(window),
                'wrong_saved_value_samples': {key: sum(v[key] != a[key] for v in values) for key in a},
                'tail_samples': len(tail), 'tail_span_seconds': tail[-1]['time'] - tail[0]['time'] if tail else 0,
                'tail_wrong_saved_value_samples': {key: sum(v[key] != a[key] for v in tail_values) for key in a},
                'tail_unresolved_references': sum(len(relationships(s)) for s in tail)}
        result['recovery_windows'][str(index + 1)] = window_result
    result['complete_sequence'] = True
    result['fault_observations_complete'] = all(w['client_gate']['samples'] > 0 for w in result['recovery_windows'].values())
    result['sample_skew_within_1_1_seconds'] = all(s['skew_seconds'] <= 1.1 for s in samples.values())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'fault-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
