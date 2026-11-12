"""Produce a pinned source index for the M0 co-op audit, not a C compiler.

Enumerates declared messages/events, references into engine C files, relevant
settings and file-scope static state. Semantic contracts are in coop_m0.md.
Comments/strings are masked for symbol references; macro expansion and indirect
calls are not inferred. References are reading routes, not proof of execution.
"""
import argparse
import bisect
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
FUNCTION = re.compile(r'^(?:static\s+|__inline\s+)?[\w *]+?\b(\w+)\s*\([^;{}]*\)\s*\{', re.M)
HOOK = re.compile(r'\b(?:network_(?:distributed\w*|objects\w*|damage\w*|actors\w*|coop\w*|game_distributed_client|game_port_vehicle_riding\w*)|coop_\w+|mcc_(?:network|checkpoint|object_names_network|load_network|admission|campaign|game_speed)\w*|third_person_protocol\w*)\b')


def mask(text):
    return TOKEN.sub(lambda match: ''.join('\n' if c == '\n' else ' ' for c in match[0]), text)


def functions(text):
    clean = mask(text)
    result = []
    for match in FUNCTION.finditer(clean):
        start = clean.index('{', match.start())
        end, depth = start + 1, 1
        while depth and end < len(clean):
            depth += (clean[end] == '{') - (clean[end] == '}')
            end += 1
        result.append((match.start(), end, match[1]))
    return result


def enum(text, anchor, prefix):
    at = text.index(anchor)
    start = text.rfind('enum', 0, at)
    body = mask(text[start:text.index('};', at)])
    value = -1
    rows = []
    for match in re.finditer(r'\b(' + re.escape(prefix) + r'\w+)\s*(?:=\s*(\d+))?\s*,', body):
        value = int(match[2]) if match[2] else value + 1
        rows.append({'name': match[1], 'id': value})
    return rows


def contract(name, path=''):
    if name.startswith('_message_'):
        return 'session'
    name = name.removeprefix('_distributed_message_')
    groups = {
        'movement': ['player_prediction', 'unit_states', 'vehicle_prediction', 'player_inputs', 'relayed_actions', 'vehicle_riding_map'],
        'objects': ['object_changes', 'object_states', 'objects_synchronized', 'client_ready'],
        'inventory': ['inventories', 'pickups'],
        'combat': ['damage_events', 'hit_reports', 'fire_origins', 'damage_animations'],
        'actors': ['actor_states', 'actor_damage'],
        'presentation': ['coop_presentation', 'coop_screen_effect', 'coop_skip_vote'],
        'world': ['structure_bsp', 'coop_device_groups', 'coop_object_names', 'coop_object_transforms', 'coop_object_looks', 'coop_device_states'],
        'events': ['coop_events', 'coop_sounds_retired'],
        'checkpoint': ['checkpoint'],
        'social': ['votekick', 'votekick_status', 'voice_up', 'voice_down', 'voice_config', 'chat_up', 'chat_down', 'chat_config', 'map_vote', 'map_vote_choice', 'notice', 'client_identity', 'client_build', 'pings'],
        'content': ['map_share_request', 'map_share_offer', 'map_share_sources', 'map_share_progress', 'mcc_share_request', 'mcc_share_response', 'mcc_admission', 'mcc_load_progress'],
        'mcc': ['mcc_grenades', 'mcc_campaign', 'mcc_object_names', 'mcc_game_speed'],
        'policies': ['aim_assist_policy', 'shield_effects'],
        'multiplayer': ['player_statistics', 'game_state'],
        'transport': ['batch'],
    }
    for group, names in groups.items():
        if name in names:
            return group
    if name.startswith('_coop_event_'):
        return 'events'
    if name.startswith('third_person_') or 'third_person' in path:
        return 'camera'
    if name.startswith('mcc_checkpoint') or 'mcc_checkpoint' in path:
        return 'checkpoint'
    if name.startswith(('mcc_admission', 'mcc_load_network')):
        return 'content'
    if name.startswith('mcc_') or '/mcc_' in path:
        return 'mcc'
    if name.startswith('coop_scripts') or 'coop_scripts' in path:
        return 'scripts'
    if any(part in name or part in path for part in ('coop_enemies', 'coop_pickups', 'coop_vehicles', 'coop_scaling')):
        return 'scaling'
    if any(part in name or part in path for part in ('sound_sync', 'spectat', 'skip', 'presentation', 'camera')):
        return 'presentation'
    if any(part in name for part in ('checkpoint', 'revert', 'restore', 'recovery', 'wipe')):
        return 'checkpoint'
    if any(part in name for part in ('friendly_fire', 'collisions', 'away', 'dedicated', 'bsp_switch', 'vehicle_riding')):
        return 'policies'
    if name.startswith('network_objects') or 'network_objects' in path:
        return 'objects'
    if name.startswith('network_damage') or 'network_damage' in path:
        return 'combat'
    if name.startswith('network_actors') or 'network_actors' in path:
        return 'actors'
    if name.startswith('network_coop_note_'):
        return 'events'
    if name.startswith('network_coop') or 'network_coop' in path:
        return 'world'
    if any(part in path for part in ('network_afk', 'network_voice', 'network_chat', 'network_votekick', 'network_map_vote', 'network_link')):
        return 'social'
    if any(part in path for part in ('network_aim_assist', 'network_riding_map', 'network_shield_effects')):
        return 'policies'
    if 'network_distributed' in name or 'network_distributed' in path:
        return 'transport'
    return 'session'


def source_files():
    # Deliberately no xbox/include or third-party source traversal.
    roots = [ROOT / 'source', ROOT / 'port/linux/game', ROOT / 'port/linux/src', ROOT / 'port/web/src']
    return sorted(path for root in roots for path in root.rglob('*')
                  if path.suffix in ('.c', '.h', '.inl', '.inc') and 'libtiff' not in path.parts)


def collect():
    texts = {path.relative_to(ROOT).as_posix(): path.read_text(encoding='latin1') for path in source_files()}
    definitions = {}
    spans = {}
    for path, text in texts.items():
        spans[path] = functions(text)
        for start, end, name in spans[path]:
            definitions.setdefault(name, []).append({'path': path, 'line': text.count('\n', 0, start) + 1})
    distributed = enum(texts['port/linux/game/network_distributed.h'], '_distributed_message_player_prediction =', '_distributed_message_')
    envelope = enum(texts['source/networking/network_messages.h'], '_message_client_broadcast_game_search =', '_message_')
    events = enum(texts['port/linux/game/network_coop.c'], '_coop_event_sound,', '_coop_event_')
    symbols = {row['name'] for row in distributed + envelope + events}
    refs = {name: [] for name in symbols}
    hooks = []
    state = []
    for path, text in texts.items():
        clean = mask(text)
        starts = [item[0] for item in spans[path]]
        def owner(at):
            i = bisect.bisect_right(starts, at) - 1
            return spans[path][i][2] if i >= 0 and at < spans[path][i][1] else '<file scope>'
        for match in re.finditer(r'\b(?:_distributed_message_|_message_|_coop_event_)\w+', clean):
            if match[0] in refs:
                refs[match[0]].append({'path': path, 'line': text.count('\n', 0, match.start()) + 1,
                                      'function': owner(match.start())})
        if path.startswith('source/') and path.endswith('.c'):
            for match in HOOK.finditer(clean):
                name = match[0]
                hooks.append({'symbol': name, 'path': path, 'line': text.count('\n', 0, match.start()) + 1,
                              'caller': owner(match.start()), 'contract': contract(name, path),
                              'definitions': definitions.get(name, [])})
        stem = Path(path).stem
        if path.startswith('port/linux/game/') and path.endswith(('.c', '.inl', '.inc')) and (
            stem.startswith(('network_', 'coop_', 'third_person_protocol')) or
            stem in ('mcc_network', 'mcc_object_names_network', 'mcc_checkpoint', 'mcc_campaign',
                     'mcc_game_speed', 'mcc_script_effects', 'mcc_load_network', 'mcc_admission')):
            for match in re.finditer(r'^static\b', clean, re.M):
                if owner(match.start()) != '<file scope>':
                    continue
                depth, end = 0, match.end()
                while end < len(clean):
                    char = clean[end]
                    depth += (char in '{(') - (char in '})')
                    if char == ';' and depth == 0:
                        break
                    end += 1
                declaration = text[match.start():end + 1]
                # Function prototypes/definitions are not state. Anonymous static structs are.
                first = re.split(r'[={;]', mask(declaration), 1)[0]
                if '(' in first or len(declaration) > 10000:
                    continue
                state.append({'path': path, 'line': text.count('\n', 0, match.start()) + 1,
                              'declaration': ' '.join(declaration.split()), 'contract': contract('', path)})
    for rows in (distributed, envelope, events):
        for row in rows:
            row['contract'] = contract(row['name'])
            row['references'] = refs[row['name']]
    settings = []
    for match in re.finditer(r'\{\s*"((?:network|server|paths)\.[^"]+|debug\.network[^"]*|game\.(?:custom_edition|mcc_[^"]*|workshop_maps)|display\.(?:third_person[^"]*|passenger_third_person))"\s*,\s*(_config_\w+)\s*,\s*("(?:\\.|[^"\\])*")\s*,\s*"([^"]+)"', texts['port/linux/src/port_config.c']):
        settings.append({'name': match[1], 'type': match[2], 'default_c_string': match[3], 'environment': match[4],
                         'line': texts['port/linux/src/port_config.c'].count('\n', 0, match.start()) + 1})
    callbacks = {}
    saved = texts['source/saved games/game_state.c']
    for name in ('before_save_procs', 'before_load_procs', 'after_load_procs'):
        body = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\};', saved, re.S)[1]
        callbacks[name] = re.findall(r'\b\w+\b', mask(body))
    return {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'scope': 'Declared message enums, direct source hook references, selected file-scope state, network/server/MCC/debug settings. Not a macro-expanded call graph.',
            'distributed_messages': distributed, 'session_messages': envelope, 'events': events,
            'engine_hooks': hooks, 'static_state': state, 'settings': settings,
            'save_callbacks': callbacks}


def render(inventory, path):
    lines = ['# M0 source index', '',
             'Generated by `tools/investigations/coop_m0_inventory.py` at the pinned',
             'revision in [the machine-readable inventory](coop_m0_inventory.json).',
             'Read [the semantic contracts and baseline record](coop_m0.md) first.',
             'This is a lexical reference index, not proof that every path ran.', '',
             '## Message and event coverage', '']
    for key, title in [('distributed_messages', 'Distributed messages'), ('session_messages', 'Session messages'), ('events', 'Co-op event variants')]:
        lines += ['### ' + title, '', '| ID | Name | Contract | Producer/consumer reading routes |', '| --- | --- | --- | --- |']
        for row in inventory[key]:
            routes = sorted(set((x['path'], x['function']) for x in row['references'] if x['function'] != '<file scope>'))
            route_text = '; '.join(f'[{Path(p).name}](<../{p}>): `{f}`' for p, f in routes)
            lines.append(f'| {row["id"]} | `{row["name"]}` | {row["contract"]} | {route_text or "Reserved/definition only; see JSON references"} |')
        lines.append('')
    lines += ['## Engine-facing symbols', '',
              'Includes references and callback registrations. `coop_respawn_effect` is',
              'an authored tag field; `network_distributed_return_addresses` is session',
              'state, not a function. Both remain visible rather than inflating the API count.', '',
              '| Symbol | Contract | Definitions | Engine callers/registrations |', '| --- | --- | --- | --- |']
    for symbol in sorted(set(x['symbol'] for x in inventory['engine_hooks'])):
        rows = [x for x in inventory['engine_hooks'] if x['symbol'] == symbol]
        definitions = '; '.join(f'[{Path(x["path"]).name}](<../{x["path"]}>)' for x in rows[0]['definitions']) or 'Data symbol'
        callers = '; '.join(f'[{Path(p).name}](<../{p}>): `{f}`' for p, f in sorted(set((x['path'], x['caller']) for x in rows)))
        lines.append(f'| `{symbol}` | {rows[0]["contract"]} | {definitions} | {callers} |')
    lines += ['', '## Settings', '',
              'Actual defaults and environment overrides from `port_config.c`; server',
              'settings include retained adjacent functionality so it cannot disappear',
              'during co-op replacement. C string escaping is preserved in the JSON.', '',
              '| Setting | Default (C literal) | Environment override |', '| --- | --- | --- |']
    for row in inventory['settings']:
        lines.append(f'| `{row["name"]}` | `{row["default_c_string"]}` | `{row["environment"]}` |')
    lines += ['', '## Saved-state callbacks', '']
    for name, callbacks in inventory['save_callbacks'].items():
        lines += ['- `' + name + '`: ' + ', '.join('`' + x + '`' for x in callbacks) + '.']
    lines += ['', 'The JSON additionally records each selected file-scope static declaration.',
              'The semantic state ledger explains checkpoint participation and reset gaps;',
              'a declaration count is not a claim that all state is saved or reset.', '']
    path.write_text('\n'.join(lines), encoding='utf8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/coop_m0_inventory.json')
    args = parser.parse_args()
    inventory = collect()
    args.output.write_text(json.dumps(inventory, indent=2) + '\n', encoding='utf8')
    render(inventory, args.output.with_suffix('.md'))
    print(json.dumps({key: len(value) for key, value in inventory.items() if isinstance(value, list)}))


if __name__ == '__main__':
    main()
