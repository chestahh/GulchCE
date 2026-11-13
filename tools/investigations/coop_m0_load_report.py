"""Publish compact local M0 load evidence and evaluate unchanged desktop gates.

Reads completed reductions, never launches a game or changes a budget. Missing
observations stay unmeasured. Allocation saturation is not a playable pass.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PILOTS = {'xbox-0', 'xbox-512-pilot', 'xbox-2048', 'xbox-8000'}


def compact(folder):
    result = json.loads((folder / 'load-results.json').read_text())
    manifest = json.loads((folder / 'manifest.json').read_text())
    host = result['boundaries']['host']
    client = result['boundaries']['client']
    first = lambda rows, kind: next((r for r in rows if r['kind'] == kind), None)
    created = first(host, 'load_created')
    counts = first(host, 'snapshot_counts')
    received = first(client, 'received_counts')
    usable = first(client, 'snapshot_usable')
    begin = first(host, 'snapshot_begin')
    snapshots = [s for s in result.get('snapshots', []) if not s.get('cached_reply')]
    pools = result.get('pool_observations', {})
    pool_rows = pools.get('host', [])
    observation = result.get('initial_snapshot_observation')
    if observation is None and counts and received and begin:
        observation = {'host_counts': counts, 'client_counts': received,
            'count_and_identity_tag_fingerprint_agree': counts['v'][1:3] == received['v'][1:3]
                and counts['v'][5] == received['v'][5],
            'begin_to_received_counts_ms': (received['time'] - begin['time']) * 1000}
    return {
        'case': folder.name, 'family': manifest['arguments']['family'],
        'classification': 'pilot' if folder.name in PILOTS else 'final_instrumentation',
        'binary_sha256': manifest['binary_sha256'], 'content': manifest['maps'],
        'seconds_requested': manifest['arguments']['seconds'], 'seconds_measured': result.get('seconds'),
        'receive_delay_ms_per_peer': manifest['arguments']['latency'], 'loss_setting_percent': manifest['arguments']['loss'],
        'requested_extra': result['requested'], 'template_kind': result['template_kind'],
        'creation': dict(zip(('requested', 'created', 'failed', 'tag_index', 'type_mask', 'fixture_ms'), created['v'])) if created else None,
        'initial_snapshot': observation, 'snapshots': snapshots,
        'pending_inventories_at_first_ready': usable['v'][2] if usable else None,
        'host_pool_initial': pool_rows[:3],
        'host_pool_unavailable_contiguous_fraction_max': max((r['v'][3] for r in pool_rows), default=None),
        'host_active_objects_max': max((r['v'][2] for r in pool_rows), default=None),
        'steady_networked_objects_range': [min(r['v'][1] for r in result['steady_live_counts']),
            max(r['v'][1] for r in result['steady_live_counts'])] if result.get('steady_live_counts') else None,
        'simulation_tick_ms': result.get('steady_tick_ms'),
        'network_tick_ms': result.get('steady_network_tick_ms'),
        'host_object_network_ms': result.get('steady_host_object_network_ms'),
        'payload': result.get('payload'), 'memory_whole_session': result['memory_whole_session'],
        'inventory_queue_peak': result.get('inventory_queue_peak'),
        'client_create_failure_counter_max': result.get('create_failure_counter_max'),
        'event_queue_peak': result.get('event_queue_peak'), 'event_overflow_observations': result.get('event_overflow_observations'),
        'correction_max_per_tick_world_units': result.get('client_state_write_max_per_observed_tick_world_units'),
        'comparison': json.loads((folder / 'comparison.json').read_text()),
        'early_exits': result['early_exits'],
        'archive_index': str((folder / 'archive-index.json').relative_to(ROOT)).replace('\\', '/'),
        'cleanup': json.loads((folder / 'cleanup.json').read_text())}


def check(case, control, budgets):
    checks = {}

    def upper(name, value, key):
        limit = budgets[key]
        checks[name] = {'observed': value, 'limit': limit, 'pass': value <= limit if value is not None else None}

    for role in ('host', 'client'):
        for field, budget in (('simulation_tick_ms', 'simulation_tick_p99_ms'), ('network_tick_ms', 'network_tick_p99_ms')):
            upper(role + '_' + field + '_p99', (case.get(field) or {}).get(role, {}).get('p99'), budget)
        sent = (case.get('payload') or {}).get(role, {}).get('sent', {})
        upper(role + '_payload_mean', sent.get('bytes_per_second'), 'steady_sent_payload_mean_bytes_per_second_per_peer')
        upper(role + '_payload_p95_second', sent.get('complete_one_second_windows_bytes', {}).get('p95'),
              'steady_sent_payload_p95_complete_second_bytes_per_peer')
        private = case['memory_whole_session'][role]['private_bytes'].get('max')
        reference = control['memory_whole_session'][role]['private_bytes'].get('max') if control else None
        upper(role + '_private_commit_delta', private - reference if private is not None and reference is not None else None,
              'private_commit_increase_over_same_map_control_bytes_per_process')
    upper('snapshot_construction_max_ms', max((s['host_construction_ms'] for s in case['snapshots']), default=None),
          'single_snapshot_construction_call_ms')
    upper('snapshot_offered_bytes_max', max((s['offered_message_bytes'] for s in case['snapshots']), default=None),
          'offered_object_inventory_snapshot_bytes')
    upper('initial_count_receipt_ms', (case.get('initial_snapshot') or {}).get('begin_to_received_counts_ms'),
          'initial_object_count_receipt_50ms_receive_delay_ms' if case['receive_delay_ms_per_peer'] else 'initial_object_count_receipt_p0_ms')
    used = case['host_pool_unavailable_contiguous_fraction_max']
    free = 1 - used if used is not None else None
    checks['pool_contiguous_headroom'] = {'observed': free, 'minimum': budgets['minimum_contiguous_object_pool_headroom_fraction'],
        'pass': free >= budgets['minimum_contiguous_object_pool_headroom_fraction'] if free is not None else None}
    upper('client_create_failures', case['client_create_failure_counter_max'], 'client_object_creation_failures')
    upper('event_overflows', case['event_overflow_observations'], 'event_queue_overflows')
    # Pending-at-ready is the snapshot_usable trace, not the live unit count.
    upper('pending_inventory_at_first_ready', case['pending_inventories_at_first_ready'],
          'pending_inventories_at_object_ready')
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sessions', type=Path, default=ROOT / 'build/m0/load')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/coop_m0_load_results.json')
    args = parser.parse_args()
    budget_path = ROOT / 'docs/coop_m0_desktop_budgets.json'
    budgets = json.loads(budget_path.read_text())
    cases = [compact(p.parent) for p in sorted(args.sessions.glob('*/load-results.json')) if (p.parent / 'cleanup.json').exists()]
    controls = {c['family']: c for c in cases if c['case'].endswith('-control')}
    for case in cases:
        control = controls.get(case['family'])
        if case['classification'] == 'final_instrumentation':
            if control and control['binary_sha256'] != case['binary_sha256']:
                raise ValueError('Memory comparison requires the same instrumented binary')
            if control and control['content'] != case['content']:
                raise ValueError('Memory comparison requires identical content')
            case['desktop_gate_checks'] = check(case, control, budgets)
    result = {'hardware': json.loads((args.sessions / 'hardware.json').read_text()),
        'budget_sha256': hashlib.sha256(budget_path.read_bytes()).hexdigest(),
        'build_manifest': json.loads((args.sessions / 'build-manifest.json').read_text()), 'cases': cases,
        'scope': 'M0 investigation only; no M1, implementation rewrite or publication',
        'blocked': ['Linux runtime: no runner', 'Android runtime: no device/emulator',
                    'Android build: previously recorded Windows shell/CMake failure'],
        'unqualified': ['complete persistent state', 'browser high-load runtime', 'public internet performance',
                        'active AI/combat capacity', 'rendered performance', 'full world memory ownership',
                        'multi-machine scaling', 'soak and repeated transitions']}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    for case in cases:
        if case['classification'] == 'pilot':
            print(case['case'], 'pilot: not gated')
            continue
        failures = [k for k,v in case.get('desktop_gate_checks', {}).items() if v['pass'] is False]
        missing = [k for k,v in case.get('desktop_gate_checks', {}).items() if v['pass'] is None]
        print(case['case'], 'budget target exceedances:', ', '.join(failures) or 'none measured',
              '; unmeasured:', ', '.join(missing) or 'none')


if __name__ == '__main__':
    main()
