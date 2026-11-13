"""Measure held ammunition and a running timer against the saved state.

Timer expiry is an absolute simulation tick, so different capture ticks do
not masquerade as disagreement. Agreement between two wrong peers is still
a failure. Expiry here means the script-visible remaining count reaches zero,
not a rendered timer or a campaign-specific expiry action.
"""
import argparse
import json
from pathlib import Path

from coop_m0_recovery_measurements import captures, one, relationships


def timer_value(state):
    timer = one(state, 'timer')
    remaining = one(state, 'timer_layout')[3]
    expected = 0 if not timer[0] else (-1 if timer[2] == -1 else (
        timer[2] if timer[1] else timer[2] + timer[3] - state['tick']))
    running = bool(timer[0] and not timer[1] and timer[2] != -1)
    return {'enabled': timer[0], 'paused': timer[1], 'remaining': remaining,
            'expired': timer[2] == -1, 'storage_ticks': timer[2], 'reference_tick': timer[3],
            'expiry_tick': state['tick'] + remaining if running else None,
            'formula_valid': remaining == expected}


def ammunition(state, weapons):
    result = []
    for unit, weapon, slot in weapons:
        membership = [r[:3] for r in state['rows'].get('inventory', []) if r[:2] == [unit, slot]]
        magazine = [r[2:4] for r in state['rows'].get('magazine', []) if r[:2] == [weapon, 0]]
        result.append({'held': membership == [[unit, slot, weapon]], 'counts': magazine})
    return result


def timer_error(state, expected_expiry):
    value = timer_value(state)
    return None if value['expiry_tick'] is None else value['expiry_tick'] - expected_expiry


def save_window(saves, request_time, changed_time, final_revert_time):
    selected = [s for s in saves if request_time <= s['time'] < changed_time]
    if len(selected) != 1 or any(changed_time <= s['time'] <= final_revert_time for s in saves):
        raise ValueError('Missing/ambiguous save or overwritten checkpoint')
    return selected[0], [s for s in saves if s['time'] > final_revert_time]


def reduce(folder):
    folder = Path(folder)
    manifest = json.loads((folder / 'manifest.json').read_text())
    controlled = manifest.get('cleanup_errors') == [] and all(
        p.get('early_exit', 'missing') is None for p in manifest['peers'].values())
    states, rows = {}, {}
    for role in ('host', 'client'):
        states[role], rows[role] = captures(folder / role, allow_truncated_tail=controlled)
    kinds = lambda kind: [r for r in rows['host'] if r['kind'] == kind]
    phases, saved, reverts, context = map(kinds, ('fixture_phase', 'timing_saved', 'timing_revert', 'timing_weapon'))
    result = {'case': folder.name, 'complete_sequence': False,
              'early_exits': {r: p['early_exit'] for r, p in manifest['peers'].items()},
              'discarded_final_captures': {r: [v for v in rows[r] if v['kind'] == 'discarded_capture'] for r in rows}}
    if [p['v'][1] for p in phases] != list(range(7)) or len(context) != 2 or len(reverts) != 2:
        result['reason'] = 'Incomplete fixture sequence, weapons or reverts'
        return result
    try:
        saved, later_saves = save_window(saved, phases[1]['time'], phases[2]['time'], reverts[-1]['time'])
    except ValueError as error:
        result['reason'] = str(error)
        return result
    weapons = [r['v'][1:4] for r in context]
    if len({w[0] for w in weapons}) != 2 or len({w[1] for w in weapons}) != 2:
        result['reason'] = 'Fixture did not select two distinct player weapons'
        return result
    selected = {'saved': max((s for s in states['host'] if phases[0]['time'] <= s['time'] < saved['time']), key=lambda s: s['time']),
                'changed': max((s for s in states['host'] if phases[2]['time'] < s['time'] < phases[3]['time']), key=lambda s: s['time'])}
    for label, phase in (('restored', 4), ('restored_again', 6)):
        selected[label] = min((s for s in states['host'] if s['time'] >= phases[phase]['time']), key=lambda s: s['time'])
    samples = {}
    for label, host in selected.items():
        client = min(states['client'], key=lambda s: abs(s['time'] - host['time']))
        samples[label] = {'skew_seconds': abs(host['time'] - client['time'])}
        for role, state in (('host', host), ('client', client)):
            samples[label][role] = {'tick': state['tick'], 'timer': timer_value(state),
                'ammunition': ammunition(state, weapons), 'epoch': one(state, 'epoch')[:2],
                'reference_failures': relationships(state)}
    expected_ammo = samples['saved']['host']['ammunition']
    changed_ammo = samples['changed']['host']['ammunition']
    result.update(weapons=weapons, save=saved, later_saves=later_saves, reverts=reverts, samples=samples,
                  mutations_observed={
                      'two_held_magazines_saved_at_20': all(v == {'held': True, 'counts': [[20, 20]]} for v in expected_ammo),
                      'two_held_magazines_changed_to_4': all(v == {'held': True, 'counts': [[4, 4]]} for v in changed_ammo),
                      'running_timer_saved': 0 < saved['v'][1] < 600 and all(samples[l]['host']['timer']['paused'] == 0 for l in samples),
                      'timer_changed': samples['changed']['host']['timer']['expiry_tick'] > samples['saved']['host']['timer']['expiry_tick']},
                  recovery_windows={})
    base_epoch = samples['saved']['host']['epoch'][0]
    result['epoch_sequence_valid'] = all(samples[label]['host']['epoch'][0] == base_epoch + offset
        and samples[label]['client']['epoch'] == [base_epoch + offset, 1]
        for label, offset in (('restored', 1), ('restored_again', 2)))
    for index, revert in enumerate(reverts):
        expected_expiry = revert['v'][0] + saved['v'][1]
        # A later autosave cannot change which save the completed reverts read.
        # Still end observation there, before any subsequent save callbacks.
        end = reverts[index + 1]['time'] if index == 0 else min(
            (s['time'] for s in later_saves), default=float('inf'))
        window_result = {'expected_expiry_tick': expected_expiry, 'clock_shift_ticks': revert['v'][1],
                         'ended_at_later_save': index == 1 and bool(later_saves), 'peers': {}}
        for role in ('host', 'client'):
            window = [s for s in states[role] if revert['time'] + 2 <= s['time'] < end]
            expiry = next((s for s in window if timer_value(s)['remaining'] <= 0), None)
            window_result['peers'][role] = {
                'samples': len(window), 'duration_seconds': window[-1]['time'] - window[0]['time'] if window else 0,
                'active_expiry_errors_ticks': sorted({timer_error(s, expected_expiry) for s in window
                                                     if timer_error(s, expected_expiry) is not None}),
                'expired_samples': sum(timer_value(s)['expired'] for s in window),
                'timer_formula_failures': sum(not timer_value(s)['formula_valid'] for s in window),
                'timer_mode_failures': sum(bool(not timer_value(s)['enabled'] or timer_value(s)['paused']) for s in window),
                'wrong_ammunition_samples': sum(ammunition(s, weapons) != expected_ammo for s in window),
                'unresolved_references': sum(len(relationships(s)) for s in window),
                'epochs': sorted({one(s, 'epoch')[0] for s in window}),
                'ready_samples': sum(one(s, 'epoch')[1] == 1 for s in window) if role == 'client' else None,
                'first_nonpositive_sample': None if expiry is None else {
                    'tick': expiry['tick'], 'actual_remaining': timer_value(expiry)['remaining'],
                    'expected_remaining': expected_expiry - expiry['tick']}}
        result['recovery_windows'][str(index + 1)] = window_result
    result['complete_sequence'] = True
    result['sample_skew_within_1_1_seconds'] = all(s['skew_seconds'] <= 1.1 for s in samples.values())
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    result = reduce(args.folder)
    (args.folder / 'timing-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
