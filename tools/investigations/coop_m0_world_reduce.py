"""Summarize local M0 observations without treating sampled state as a pass."""
import argparse
from collections import Counter, defaultdict
from contextlib import contextmanager
import csv
import io
import json
from pathlib import Path
import zipfile


@contextmanager
def evidence(folder, role, name):
    path = folder / role / name
    if path.exists():
        with path.open(encoding='utf-8') as stream:
            yield stream
    else:
        with zipfile.ZipFile(folder / 'evidence.zip') as archive:
            with archive.open(f'{role}/{name}') as raw:
                with io.TextIOWrapper(raw, encoding='utf-8') as stream:
                    yield stream


def records(stream):
    """Reject truncated rows and non-finite values, including unrelated events."""
    import math
    for line, row in enumerate(csv.reader(stream), 1):
        if len(row) != 8:
            raise ValueError(f'Incomplete observation row {line}')
        values = [float(value) for value in [row[0], *row[2:]]]
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f'Non-finite observation row {line}')
        yield values[0], row[1], values[1:]


def frames(rows, events):
    """Pair boundaries by occurrence, never merge ticks repeated after revert."""
    current = None
    for wall, kind, values in rows:
        tick = int(values[0])
        if kind == 'world_begin':
            if current is not None:
                raise ValueError('Missing world_end')
            current = dict(wall=wall, tick=tick, bsp=int(values[1]),
                           cinematic=bool(values[3]), input=bool(values[4]), objects={})
        elif kind == 'world_end':
            if current is None or current['tick'] != tick:
                raise ValueError('Unpaired world_end')
            yield current
            current = None
        elif kind in ('world_plan_start', 'world_command', 'world_screenshot', 'world_vote', 'checkpoint_saved'):
            events.append(dict(wall=wall, kind=kind, values=values))
        elif kind.startswith('world_'):
            if current is None or current['tick'] != tick:
                raise ValueError('World sample outside matching frame')
            handle = int(values[1])
            obj = current['objects'].setdefault(handle, {})
            if kind == 'world_device_ray':
                obj.setdefault('ray', []).append(values[2:])
            else:
                field = kind.removeprefix('world_')
                if field in obj:
                    raise ValueError('Duplicate object field')
                obj[field] = values[2:]
    if current is not None:
        events.append(dict(wall=current['wall'], kind='incomplete_final_frame',
                           values=[current['tick']]))


def summarize(stream, selected_name=None):
    events, samples = [], []
    counts = Counter()
    gameplay = Counter()
    identities = defaultdict(set)
    extents = {}
    previous_tick = None
    rewinds = []
    epoch = 0
    for frame in frames(records(stream), events):
        if previous_tick is not None and frame['tick'] < previous_tick:
            rewinds.append(dict(wall=frame['wall'], before=previous_tick, after=frame['tick']))
            epoch += 1
        previous_tick = frame['tick']
        compact = {key: value for key, value in frame.items() if key != 'objects'}
        compact.update(players={}, device=None)
        for handle, obj in frame['objects'].items():
            if not {'object', 'pose', 'health'} <= obj.keys():
                raise ValueError('Incomplete object observation')
            typ, name, parent, cluster = map(int, obj['object'])
            identities[str(typ)].add(handle)
            pose = obj['pose'][:3]
            if typ in (0, 1):
                if not {'control', 'throttle'} <= obj.keys():
                    raise ValueError('Missing unit state')
                player, actor, control, seat = map(int, obj['control'])
                if player != -1:
                    if 'floor' not in obj:
                        raise ValueError('Missing player floor probe')
                    floor = list(obj['floor'])
                    # Early observer builds sampled this union field for structure hits.
                    # It is defined only for object collisions; never compare the other bytes.
                    if floor[1] != 3:
                        floor[2] = -1
                    compact['players'][str(player)] = dict(handle=handle, xyz=pose,
                        body=obj['health'][0], shield=obj['health'][1], parent=parent,
                        seat=seat, cluster=cluster, damage_flags=int(obj['pose'][3]),
                        floor=floor, control=control, throttle=obj['throttle'][:3])
                    counts['player_seated_samples'] += parent != -1 and seat != -1
                    counts['player_control_samples'] += control != 0
                    counts['player_throttle_samples'] += any(obj['throttle'][:3])
                    if not frame['cinematic'] and frame['input']:
                        gameplay['player_seated_samples'] += parent != -1 and seat != -1
                        gameplay['player_control_samples'] += control != 0
                        gameplay['player_throttle_samples'] += any(obj['throttle'][:3])
                if actor != -1:
                    identities['actors'].add(handle)
                    if not frame['cinematic']:
                        identities['noncinematic_actors'].add(handle)
                if not frame['cinematic']:
                    key = (epoch, handle, typ, parent, frame['bsp'])
                    low, high = extents.setdefault(key, (list(pose), list(pose)))
                    for i, value in enumerate(pose):
                        low[i] = min(low[i], value)
                        high[i] = max(high[i], value)
            if selected_name is not None and name == selected_name and 'device' in obj:
                compact['device'] = dict(handle=handle, name=name, xyz=pose,
                    cluster=cluster, values=obj['device'], rays=obj.get('ray', []))
        samples.append(compact)
    if not samples:
        raise ValueError('No complete world frames')
    for typ, label in ((0, 'biped'), (1, 'vehicle')):
        gameplay[f'{label}_extents_over_one_unit'] = sum(
            key[2] == typ and max(b-a for a, b in zip(low, high)) > 1
            for key, (low, high) in extents.items())
    return dict(frames=len(samples), first_wall=samples[0]['wall'],
                last_wall=samples[-1]['wall'], counts=dict(counts), gameplay=dict(gameplay),
                unique_observed={kind: len(handles) for kind, handles in identities.items()},
                rewinds=rewinds, events=events, samples=samples)


def reduce_case(folder):
    config = json.loads(folder.with_name(folder.name + '-command.json').read_text())['fixture']
    result = dict(case=folder.name, fixture=config, roles={})
    for role in ('host', 'client'):
        selected = None
        if config.get('device'):
            with evidence(folder, role, 'm0-world-metadata.tsv') as stream:
                selected = next((int(r[1]) for r in csv.reader(stream, delimiter='\t')
                                 if r[0] == 'name' and r[2] == config['device']), None)
            if selected is None:
                raise ValueError('Selected device name absent from metadata')
        with evidence(folder, role, 'm0-network_coop.csv') as stream:
            result['roles'][role] = summarize(stream, selected)
        transitions = []
        previous = None
        with evidence(folder, role, 'm0-network_distributed.csv') as stream:
            for wall, kind, values in records(stream):
                if kind == 'epoch':
                    state = values[1:3]
                    if state != previous:
                        transitions.append(dict(wall=wall, tick=values[0], epoch=state[0], ready=state[1]))
                        previous = state
        result['roles'][role]['recovery_transitions'] = transitions
    host_events = result['roles']['host']['events']
    windows = []
    for event in host_events:
        if event['kind'] != 'world_command':
            continue
        window = dict(sequence=int(event['values'][1]), wall=event['wall'], peers={})
        for role, data in result['roles'].items():
            points = {}
            for offset in (-1, 1, 5, 10):
                target = event['wall'] + offset
                nearest = min(data['samples'], key=lambda sample: abs(sample['wall'] - target))
                points[str(offset)] = nearest if abs(nearest['wall'] - target) < 0.5 else None
            window['peers'][role] = points
        windows.append(window)
    result['command_windows'] = windows
    for role, data in result['roles'].items():
        samples = data.pop('samples')
        data['last_sample'] = samples[-1]
        data['bsp_changes'] = [sample for index, sample in enumerate(samples)
            if index == 0 or sample['bsp'] != samples[index-1]['bsp']]
        data['device_changes'] = []
        previous = None
        for sample in samples:
            device = sample['device']
            # Keep stable endpoint changes and presence loss. Interpolation remains in raw traces.
            state = None if device is None else (round(device['values'][1], 2),
                tuple(tuple(ray) for ray in device['rays']))
            if state != previous:
                data['device_changes'].append(sample)
                previous = state
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    output = reduce_case(args.folder)
    (args.folder / 'world-summary.json').write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps({role: {k: v for k, v in data.items()
        if k in ('frames', 'counts', 'unique_observed', 'rewinds')}
        for role, data in output['roles'].items()}, indent=2))
