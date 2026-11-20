"""Check capture-only source marker ownership, independently of silhouette size."""
import argparse
import json
from pathlib import Path
import re

SAMPLE = re.compile(r'SOURCE POSE (\w+) tick (\d+) third ([01]) fp ([01]) world_error ([\d.]+) fp_error ([-\d.]+) marker ([^\r\n]+)')


def check(directory, switching=False):
    manifest = json.loads((directory/'build-manifest.json').read_text())
    if not manifest.get('source_pose_diagnostics'):
        raise ValueError('Not a source-pose diagnostic capture')
    report = json.loads((directory/'report.json').read_text())
    logs = (directory/'stderr.log').read_text(errors='replace')
    samples = SAMPLE.findall(logs)
    if not samples:
        raise ValueError('No source samples')
    modules = {}
    for name, tick, third, fp, world_error, fp_error, marker in samples:
        third, fp = int(third), int(fp)
        expected = float(world_error if third or not fp else fp_error)
        if not 0 <= expected <= 0.00001:
            raise ValueError(f'{name} tick {tick}: incorrect marker, error {expected}')
        entry = modules.setdefault(name, dict(samples=0, world=0, first_person=0, max_error=0))
        entry['samples'] += 1
        entry['world' if third or not fp else 'first_person'] += 1
        entry['max_error'] = max(entry['max_error'], expected)
    expected_modules = set()
    if report.get('muzzle'):
        expected_modules.add('muzzle_lighting')
    emitter = report.get('emitter') or ''
    if emitter.startswith('ammo-'):
        expected_modules.add('ammo_counter_lighting')
    elif emitter.startswith('needler-'):
        expected_modules.add('needler_lighting')
    if not expected_modules.issubset(modules):
        raise ValueError('Missing source modules: ' + str(expected_modules - modules.keys()))
    if report['exit'] or report['shadow_shader_failures'] or report['shadow_incomplete_samples']:
        raise ValueError('Capture/render failure')
    if switching:
        if re.findall(r'SHADOW CAMERA tick (\d+) third (\d+)', logs) != [('110','1'),('170','0')]:
            raise ValueError('Missing camera transitions')
        intervals = re.split(r'SHADOW CAMERA tick \d+ third [01]', logs)
        for name in modules:
            for i, interval in enumerate(intervals):
                selected = [r for r in SAMPLE.findall(interval) if r[0] == name and int(r[2]) == (i == 1)]
                if not selected:
                    raise ValueError(f'{name}: missing source evidence in camera interval {i}')
        for i in (0,2):
            if not re.search(r'SHADOW selected[^\r\n]+ view_weapon (?!-1\b)-?\d+', intervals[i]):
                raise ValueError('Missing first-person shadow geometry evidence')
        if not re.search(r'SHADOW selected[^\r\n]+ view_weapon -1 view_hands -1', intervals[1]):
            raise ValueError('Missing shoulder shadow geometry evidence')
    result = dict(directory=str(directory), modules=modules,
                  cap=report['cap'], average_fps=report['average_fps'],
                  pacing_pass=report['average_fps'] >= report['cap']*.95)
    (directory/'source-pose-report.json').write_text(json.dumps(result, indent=2))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', nargs='+', type=Path)
    parser.add_argument('--switching', action='store_true')
    args = parser.parse_args()
    for directory in args.directories:
        print(json.dumps(check(directory, args.switching)))


if __name__ == '__main__':
    main()
