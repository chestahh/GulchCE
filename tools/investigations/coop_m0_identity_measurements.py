"""Observe salted object-slot reuse and saved inventory ownership on real peers.

The oracle requires the original full handle and definition, not merely the
same physical slot. It checks explicit absence of all post-save allocations.
"""
import argparse
import json
from pathlib import Path

from coop_m0_recovery_measurements import captures, compare_fields, one, relationships
from coop_m0_timing_measurements import save_window


def reused_slot(old, new):
    return old != -1 and new != -1 and old != new and (int(old) & 0xffff) == (int(new) & 0xffff)


def state_values(state, units, original, replacements):
    rows = state['rows']
    objects = rows.get('object', [])
    tracked = [original, *replacements]
    present = {r[0] for r in objects}
    return {
        'slot_contents': [r[:5] for r in objects if (int(r[0]) & 0xffff) == (int(original) & 0xffff)],
        'post_save_presence': [index in present for index in replacements],
        'inventories': sorted(r[:3] for r in rows.get('inventory', []) if r[0] in units),
        'all_tracked_inventory_references': sorted(r[:3] for r in rows.get('inventory', []) if r[2] in tracked),
        'owners': sorted(r[:4] for r in rows.get('identity_owner', []) if r[0] in tracked),
        'primary_magazines': sorted(r[:4] for r in rows.get('magazine', []) if r[0] in tracked and r[1] == 0),
    }


def mutation_checks(a, b, units, players, definitions, original, replacement):
    def owns(value, unit, weapon):
        return value['all_tracked_inventory_references'] == [[unit, 0, weapon]]
    return {
        'same_slot_different_generation': reused_slot(original, replacement),
        'different_weapon_definition': definitions[0] != definitions[1],
        'saved_identity': len(a['slot_contents']) == 1 and a['slot_contents'][0][:2] == [original, definitions[0]],
        'replacement_identity': len(b['slot_contents']) == 1 and b['slot_contents'][0][:2] == [replacement, definitions[1]],
        'saved_owner_inventory': owns(a, units[0], original),
        'replacement_owner_inventory': owns(b, units[1], replacement),
        'saved_owner_metadata': [original, players[0], 1, 1] in a['owners'],
        'replacement_owner_metadata': [replacement, players[1], 1, 1] in b['owners'],
        'saved_primary_rounds': [original, 0, 20, 20] in a['primary_magazines'],
        'replacement_primary_rounds': [replacement, 0, 4, 4] in b['primary_magazines'],
        'post_save_absence_at_save': not any(a['post_save_presence']),
    }


def ready_observations(states, start, end, epoch, units, original, replacements, expected):
    ready = [s for s in states if start <= s['time'] < end and one(s, 'epoch')[:2] == [epoch, 1]]
    values = [state_values(s, units, original, replacements) for s in ready]
    return {'samples': len(ready),
            'first_capture_after_revert_seconds': ready[0]['time'] - start if ready else None,
            'wrong_saved_value_samples': {key: sum(v[key] != expected[key] for v in values) for key in expected},
            'unresolved_references': sum(len(relationships(s)) for s in ready)}


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    controlled = manifest.get('cleanup_errors') == [] and all(
        p.get('early_exit', 'missing') is None for p in manifest['peers'].values())
    states, rows = {}, {}
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled)
    kinds = lambda kind: [r for r in rows['host'] if r['kind'] == kind]
    phases, reverts, context, allocations, additions = map(kinds, (
        'fixture_phase', 'identity_revert', 'identity_context', 'identity_new', 'identity_add'))
    result = {'case': folder.name, 'complete_sequence': False,
              'early_exits': {r: p['early_exit'] for r, p in manifest['peers'].items()},
              'discarded_final_captures': {r: [v for v in rows[r] if v['kind'] == 'discarded_capture'] for r in rows},
              'allocations': allocations, 'additions': additions, 'reverts': reverts}
    if [p['v'][1] for p in phases] != list(range(7)) or len(context) != 1 or len(reverts) != 2:
        result['reason'] = 'Incomplete fixture sequence, context or reverts'
        return result
    try:
        saved, later_saves = save_window(kinds('checkpoint_saved'), phases[1]['time'], phases[2]['time'], reverts[-1]['time'])
    except ValueError as error:
        result['reason'] = str(error)
        return result
    if len(additions) != 2 or [r['v'][1] for r in additions] != [0, 1] or any(r['v'][4] != 1 for r in additions):
        result['reason'] = 'Inventory addition failed or is missing'
        return result
    original, replacement = [r['v'][3] for r in additions]
    replacements = [r['v'][2] for r in allocations if r['v'][1] == 1]
    if not replacements or -1 in replacements or replacement not in replacements:
        result['reason'] = 'Missing replacement allocation'
        return result
    units, definitions = context[0]['v'][1:3], context[0]['v'][3:5]
    selected = {'saved': max((s for s in states['host'] if phases[0]['time'] <= s['time'] < saved['time']), key=lambda s: s['time']),
                'changed': max((s for s in states['host'] if phases[2]['time'] < s['time'] < phases[3]['time']), key=lambda s: s['time'])}
    for label, phase in (('restored', 4), ('restored_again', 6)):
        selected[label] = min((s for s in states['host'] if s['time'] >= phases[phase]['time']), key=lambda s: s['time'])
    samples = {}
    for label, host in selected.items():
        client = min(states['client'], key=lambda s: abs(s['time'] - host['time']))
        samples[label] = {'skew_seconds': abs(host['time'] - client['time'])}
        for role, state in (('host', host), ('client', client)):
            samples[label][role] = {'tick': state['tick'], 'values': state_values(state, units, original, replacements),
                'epoch': one(state, 'epoch')[:2], 'roster': state['rows'].get('roster', []),
                'reference_failures': relationships(state)}
    a, b = [samples[label]['host']['values'] for label in ('saved', 'changed')]
    players = [next(r[0] for r in samples['saved']['host']['roster'] if r[1] == unit) for unit in units]
    result.update(original=original, replacement=replacement, replacement_allocations=len(replacements),
                  slot=int(original) & 0xffff, units=units, players=players, definitions=definitions, save=saved, later_saves=later_saves,
                  samples=samples, mutations_observed=mutation_checks(a, b, units, players, definitions, original, replacement),
                  client_mutations_observed=mutation_checks(samples['saved']['client']['values'],
                      samples['changed']['client']['values'], units, players, definitions, original, replacement),
                  restoration_failures={label: {role: compare_fields(a, samples[label][role]['values'], a)
                      for role in ('host', 'client')} for label in ('restored', 'restored_again')}, recovery_windows={})
    base_epoch = samples['saved']['host']['epoch'][0]
    result['epoch_sequence_valid'] = all(samples[label]['host']['epoch'][0] == base_epoch + offset
        and samples[label]['client']['epoch'] == [base_epoch + offset, 1]
        for label, offset in (('restored', 1), ('restored_again', 2)))
    for index, revert in enumerate(reverts):
        end = reverts[index + 1]['time'] if index == 0 else min((s['time'] for s in later_saves), default=float('inf'))
        window_result = {'ended_at_later_save': index == 1 and bool(later_saves), 'peers': {}}
        window_result['client_ready_observations'] = ready_observations(states['client'], revert['time'], end,
            base_epoch + index + 1, units, original, replacements, a)
        for role in ('host', 'client'):
            window = [s for s in states[role] if revert['time'] + 2 <= s['time'] < end]
            values = [state_values(s, units, original, replacements) for s in window]
            window_result['peers'][role] = {
                'samples': len(window), 'duration_seconds': window[-1]['time'] - window[0]['time'] if window else 0,
                'wrong_saved_value_samples': {key: sum(v[key] != a[key] for v in values) for key in a},
                'unresolved_references': sum(len(relationships(s)) for s in window),
                'epochs': sorted({one(s, 'epoch')[0] for s in window}),
                'ready_samples': sum(one(s, 'epoch')[1] == 1 for s in window) if role == 'client' else None}
        result['recovery_windows'][str(index + 1)] = window_result
    result['complete_sequence'] = True
    result['sample_skew_within_1_1_seconds'] = all(s['skew_seconds'] <= 1.1 for s in samples.values())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'identity-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
