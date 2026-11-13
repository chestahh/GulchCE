"""Characterize changed persistent domains using the actual saved engine state.

Fixture IDs identify both required presence and required absence. Readiness is
recorded separately. These exact comparisons use paused timers and dyadic values;
they do not equate moving poses or certify rendered/audio behavior.
"""
import argparse
import json
from pathlib import Path

from coop_m0_recovery_measurements import captures, compare_fields, one, relationships


def state_values(state, unit, ids):
    rows = state['rows']
    objects = {r[0]: r for r in rows.get('object', [])}
    poses = {r[0]: r for r in rows.get('object_pose', [])}
    timer = one(state, 'timer')
    result = {
        'presence': [ids[i] in objects for i in range(5)],
        'identity': [objects.get(ids[i], [])[:4] for i in range(5)],
        'attachment': objects.get(ids[2], [None] * 5)[4],
        'scale': poses.get(ids[3], [None] * 5)[4],
        # Only primary totals/loaded counts are exercised. A second storage
        # slot need not be an authored magazine; its state is not an oracle.
        'magazines': [r[1:4] for r in rows.get('magazine', []) if r[0] == ids[3] and r[1] == 0],
        'inventory': [r[:3] for r in rows.get('inventory', []) if r[0] == unit],
        'timer': [timer[0], timer[1], timer[2], timer[4]],
        'timer_layout': one(state, 'timer_layout')[:4],
        'allegiance': [[r[0], int(r[1]) & 1, int(r[2]) & 1] for r in rows.get('allegiance', []) if r[0] == 3],
        'mcc_state': rows.get('mcc_state', []),
        'mcc_extra_grenades': [r[:3] for r in rows.get('mcc_grenades', []) if r[0] == unit],
    }
    return result


def mutation_checks(a, b, empty, ids):
    """Prove intended engine effects, not merely that some fields differ."""
    held = lambda value: [r[2] for r in value['inventory'] if r[2] != -1]
    checks = {
        'saved_object_deleted': a['presence'][0] and not b['presence'][0],
        'post_save_object_created': not a['presence'][4] and b['presence'][4],
        'attachment_removed': a['attachment'] == ids[1] and b['attachment'] == -1,
        'scale_changed': a['scale'] == .5 and b['scale'] == 2,
        'ammunition_changed': bool(a['magazines']) and a['magazines'] != b['magazines'],
        'inventory_changed': (not held(a) and held(b) == [ids[4]]) if empty else bool(held(a)) and not held(b),
        'paused_visible_timer_changed': a['timer'][:2] == b['timer'][:2] == [1, 1] and a['timer'][2:] != b['timer'][2:],
        'timer_layout_changed': a['timer_layout'][:3] != b['timer_layout'][:3],
        'allegiance_changed': a['allegiance'] == [[3, 1, 1]] and b['allegiance'] == [[3, 0, 0]],
    }
    if a['mcc_state']:
        checks.update(mcc_speed_changed=a['mcc_state'][0][0] == .75 and b['mcc_state'][0][0] == .5,
                      mcc_gains_changed=a['mcc_state'][0][1:3] == [.25, .5] and b['mcc_state'][0][1:3] == [.75, .875])
        if a['mcc_state'][0][3] > 2:
            checks['mcc_extra_grenades_changed'] = a['mcc_extra_grenades'] != b['mcc_extra_grenades']
    return checks


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    controlled_stop = manifest.get('cleanup_errors') == [] and all(
        'early_exit' in p and p['early_exit'] is None for p in manifest['peers'].values())
    states, rows = {}, {}
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled_stop)
    phases = [r for r in rows['host'] if r['kind'] == 'fixture_phase']
    contexts = [r for r in rows['host'] if r['kind'] == 'domain_context']
    saves = [r for r in rows['host'] if r['kind'] == 'checkpoint_saved' and r['v'][1] == 1]
    result = {'case': folder.name, 'complete_sequence': False, 'phases': phases, 'save_callbacks': saves,
              'early_exits': {role: entry['early_exit'] for role, entry in manifest['peers'].items()},
              'discarded_final_captures': {role: [r for r in rows[role] if r['kind'] == 'discarded_capture'] for role in rows}}
    if [p['v'][1] for p in phases] != list(range(7)) or len(contexts) != 1:
        result['reason'] = 'Incomplete fixture sequence/context'
        return result
    saved = [r for r in saves if phases[1]['time'] <= r['time'] < phases[2]['time']]
    if len(saved) != 1 or any(phases[2]['time'] <= s['time'] <= phases[6]['time'] for s in saves):
        result['reason'] = 'Missing/ambiguous save or overwritten checkpoint'
        return result
    ids = {int(r['v'][1]): int(r['v'][2]) for r in rows['host'] if r['kind'] == 'domain_object'}
    if set(ids) != set(range(5)) or -1 in ids.values():
        result['reason'] = 'Missing fixture object allocation'
        return result
    unit, definition, empty, grenade_types = contexts[0]['v'][1:5]
    selected = {
        'saved': max((s for s in states['host'] if phases[0]['time'] <= s['time'] < saved[0]['time']), key=lambda s: s['time']),
        'changed': max((s for s in states['host'] if phases[2]['time'] < s['time'] < phases[3]['time']), key=lambda s: s['time'])}
    for label, phase in (('restored', 4), ('restored_again', 6)):
        selected[label] = min((s for s in states['host'] if s['time'] >= phases[phase]['time']), key=lambda s: s['time'])
    result.update(fixture_ids=ids, unit=unit, definition=definition, empty_inventory_at_save=bool(empty),
                  mcc_grenade_types=grenade_types, samples={}, recovery_windows={})
    for label, host in selected.items():
        client = min(states['client'], key=lambda s: abs(s['time'] - host['time']))
        pair = {role: state_values(state, unit, ids) for role, state in (('host', host), ('client', client))}
        pair['sample_skew_seconds'] = abs(client['time'] - host['time'])
        pair['epochs'] = {role: one(state, 'epoch')[:2] for role, state in (('host', host), ('client', client))}
        pair['ticks'] = {'host': host['tick'], 'client': client['tick']}
        pair['references'] = {role: relationships(state) for role, state in (('host', host), ('client', client))}
        pair['peer_disagreement'] = compare_fields(pair['host'], pair['client'], pair['host'])
        result['samples'][label] = pair
    a = result['samples']['saved']['host']
    result['mutations_observed'] = mutation_checks(a, result['samples']['changed']['host'], bool(empty), ids)
    result['restoration_failures'] = {label: {role: compare_fields(a, result['samples'][label][role], a)
        for role in ('host', 'client')} for label in ('restored', 'restored_again')}
    base_epoch = result['samples']['saved']['epochs']['host'][0]
    result['epoch_sequence_valid'] = all(result['samples'][label]['epochs']['host'][0] == base_epoch + offset
        and result['samples'][label]['epochs']['client'] == [base_epoch + offset, 1]
        for label, offset in (('restored', 1), ('restored_again', 2)))
    for label, start, end in (('first', phases[3]['time'] + 2, phases[5]['time']),
                              ('second', phases[5]['time'] + 2, float('inf'))):
        result['recovery_windows'][label] = {}
        for role in ('host', 'client'):
            window = [s for s in states[role] if start <= s['time'] < end]
            values = [state_values(s, unit, ids) for s in window]
            result['recovery_windows'][label][role] = {
                'samples': len(values), 'duration_seconds': window[-1]['time'] - window[0]['time'] if window else 0,
                'epochs': sorted({one(s, 'epoch')[0] for s in window}),
                'ready_samples': sum(one(s, 'epoch')[1] == 1 for s in window) if role == 'client' else None,
                'wrong_saved_value_samples': {key: sum(v[key] != a[key] for v in values) for key in a}}
    result['complete_sequence'] = True
    result['sample_skew_within_1_1_seconds'] = all(p['sample_skew_seconds'] <= 1.1 for p in result['samples'].values())
    result['limits'] = 'Bounded engine mutations, not complete world, presentation/audio, combat or platform acceptance.'
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'domain-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result.get(k) for k in ('case', 'complete_sequence', 'reason', 'mutations_observed', 'restoration_failures')}))
