"""Generate a non-shipping Windows real-engine snapshot load fixture.

M0_LOAD_OBJECTS requests extra objects immediately before the first client
snapshot. M0_LOAD_BIPEDS selects unowned bipeds instead of weapons. No AI is
attached by the fixture. Allocation stops at the first failure. Counts and
identity/tag hashes distinguish created objects from actually retained ones.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import coop_m0_trace as trace

OUTPUT = trace.ROOT / 'build/m0/load/generated'
trace.OUTPUT = OUTPUT


def instrument(relative, original):
    if relative == 'source/game/game.c':
        text = trace.HEADER.replace('TRACE_NAME', 'game') + original
        text = trace.once(text, '\treal seconds_per_tick;\n\n\tprofile_tick_start();',
                          '\treal seconds_per_tick;\n\tdouble m0_started = coop_m0_clock();\n\n\tprofile_tick_start();')
        return trace.once(text, '\tprofile_tick_end();', '\tprofile_tick_end();\n'
            '\tm0_emit("tick_ms", game_time_get(), (coop_m0_clock() - m0_started) * 1000.0, game_connection(), 0, 0, 0);')
    text = trace.instrument(relative, original)
    if Path(relative).stem == 'network_distributed':
        text = trace.once(text, 'void network_distributed_tick(\n\tvoid)\n{\n\tshort connection = game_connection();',
            'void network_distributed_tick(\n\tvoid)\n{\n\tshort connection = game_connection();\n\tdouble m0_started = coop_m0_clock();')
        return trace.once(text, '\tdistributed_machines.valid = FALSE;\n}\n\n/* whether a message',
            '\tdistributed_machines.valid = FALSE;\n'
            '\tm0_emit("network_tick_ms", game_time_get(), (coop_m0_clock() - m0_started) * 1000.0, connection, 0, 0, 0);\n}\n\n/* whether a message')
    if Path(relative).stem != 'network_objects':
        return text
    fixture = Path(__file__).with_suffix('.c').read_text()
    text = trace.once(text, 'void network_objects_client_asked(', fixture + '\nvoid network_objects_client_asked(')
    text = trace.once(text, '\tm0_emit("snapshot_begin",', '\tm0_load_create();\n\tm0_load_count("snapshot_counts");\n\tm0_emit("snapshot_begin",')
    text = trace.once(text, '\tobjects_client_synchronized = TRUE;',
        '\tobjects_client_synchronized = TRUE;\n\tm0_load_count("received_counts");')
    text = trace.once(text, '\tprofile_scope(objects_host_tick_update_objects_section,',
        '\tdouble m0_started = coop_m0_clock();\n\tprofile_scope(objects_host_tick_update_objects_section,')
    text = trace.once(text, '\tmcc_network_host_tick();', '\tmcc_network_host_tick();\n'
        '\tm0_emit("host_objects_ms", game_time_get(), (coop_m0_clock() - m0_started) * 1000.0, 0, 0, 0, 0);\n'
        '\tif (game_time_get() % 30 == 0) m0_load_count("live_counts");')
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=trace.ROOT, text=True).strip(),
                'files': {}, 'fixture_sha256': hashlib.sha256(Path(__file__).with_suffix('.c').read_bytes()).hexdigest()}
    for relative in ('port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
                     'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
                     'port/linux/game/network_test.c', 'source/game/game.c'):
        source = trace.ROOT / relative
        original = source.read_bytes()
        target = OUTPUT / source.name
        target.write_text(instrument(relative, original.decode('utf8').replace('\r\n', '\n')), encoding='utf8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUTPUT / 'compile.cmd').write_text(f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}" compile %*\n')
    print(OUTPUT / 'compile.cmd')


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'compile':
        sys.exit(trace.compile_source(sys.argv[2:]))
    generate()
