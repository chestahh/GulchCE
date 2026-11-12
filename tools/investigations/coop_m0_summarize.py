"""Summarize retained M0 observations without changing the comparison oracle.

Read session manifests/comparisons, process-memory samples, JUnit reports and
Chrome profile parts. Missing final parts remain explicitly incomplete. Run
before archiving profile parts, or restore them to their original relative paths.
"""
import argparse
import collections
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


SCOPES = ('game_tick', 'network_distributed_tick',
          'network_distributed_tick.coop_host_tick',
          'network_distributed_tick.actors_host_tick')


def distribution(values):
    values = sorted(values)
    return {'count': len(values), 'mean_ms': sum(values) / len(values),
            **{f'p{n}_ms': values[math.ceil(n / 100 * len(values)) - 1]
               for n in (50, 95, 99)}, 'max_ms': max(values)}


def summarize(root):
    output = {'sessions': {}, 'tests': {}, 'profiles': {}}
    for path in sorted((root / 'sessions').glob('*/comparison.json')):
        folder = path.parent
        manifest = json.loads((folder / 'manifest.json').read_text())
        samples = json.loads((folder / 'observations.json').read_text())
        memory = {}
        for role in ('host', 'client'):
            # Beyond the first three player-1 log records; not proof of a live unit.
            values = [x[role] for x in samples if x[role]['records'] > 3]
            memory[role] = {
                key: {'min': min(x[key] for x in values), 'max': max(x[key] for x in values)}
                for key in ('working_bytes', 'private_bytes')
                if values and all(key in x for x in values)}
        output['sessions'][folder.name] = {
            'revision': manifest['revision'], 'binary_sha256': manifest['binary_sha256'],
            'elapsed_seconds': manifest['elapsed_seconds'],
            'comparison': json.loads(path.read_text()), 'process_memory': memory,
            'map_identities': manifest['maps']}
    final_tests = {}
    # Later invocation replaces the same testcase's earlier result, not all results.
    for filename in ('baseline-tests.xml', 'invocation-tests.xml'):
        path = root / filename
        if not path.exists():
            continue
        counts = collections.Counter()
        failures = collections.Counter()
        for test in ET.parse(path).iter('testcase'):
            status = ('failed' if test.find('failure') is not None or test.find('error') is not None
                      else 'skipped' if test.find('skipped') is not None else 'passed')
            final_tests[(test.get('classname'), test.get('name'))] = status
            counts[status] += 1
            if status == 'failed':
                failures[test.get('classname')] += 1
        output['tests'][filename] = {'counts': counts, 'failures_by_suite': failures}
    output['tests']['final_unique_counts'] = collections.Counter(final_tests.values())
    for folder in sorted((root / 'sessions').glob('*/*/profiles')):
        recordings = collections.defaultdict(lambda: {'headers': [], 'scopes': collections.defaultdict(list)})
        for path in sorted(folder.glob('*.json')):
            data = json.loads(path.read_text())
            header = data['halo']['header']
            recording = recordings[header['recording']]
            recording['headers'].append(header)
            for event in data['traceEvents']:
                if event.get('ph') == 'X' and event.get('name') in SCOPES:
                    # Chrome trace times are microseconds relative to recording start.
                    if event.get('ts', 0) >= 5_000_000:
                        recording['scopes'][event['name']].append(event['dur'] / 1000)
        for name, recording in recordings.items():
            headers = recording['headers']
            parts = sorted(h['part'] for h in headers)
            output['profiles'][folder.parent.parent.name + '/' + folder.parent.name + '/' + name] = {
                'map': headers[0]['map'],
                'complete': (any(h['last_part'] for h in headers)
                             and parts == list(range(1, parts[-1] + 1))),
                'parts': len(headers), 'seconds_in_parts': sum(h['duration_s'] for h in headers),
                'dropped_scopes': sum(h['dropped_scopes'] for h in headers),
                'scopes_after_5_seconds': {name: distribution(values)
                                         for name, values in recording['scopes'].items() if values}}
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('build/m0'))
    parser.add_argument('--output', type=Path, default=Path('docs/coop_m0_results.json'))
    args = parser.parse_args()
    result = summarize(args.root)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(json.dumps({'sessions': len(result['sessions']), 'profiles': len(result['profiles']),
                      'tests': result['tests']['final_unique_counts']}))


if __name__ == '__main__':
    main()
