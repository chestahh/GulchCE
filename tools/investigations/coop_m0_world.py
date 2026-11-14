"""Generate private active-world observations; never edit production sources."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import coop_m0_recovery as recovery
import coop_m0_trace as trace

OUTPUT = trace.ROOT / 'build/m0/active-world/generated'
trace.OUTPUT = OUTPUT


def instrument(relative, original):
    name = Path(relative).stem
    if name == 'players':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        anchor = '\tresult = player_teleport_internal(player_index, source_unit_index, position);'
        return trace.once(text, anchor, '\tm0_emit("teleport_request", game_time_get(), player_index, position->x, position->y, position->z, global_structure_bsp_index_get());\n' + anchor + '\n'
            '\t\tm0_emit("teleport_result", game_time_get(), player_index, result, biped->object.position.x, biped->object.position.y, biped->object.position.z);')
    if name == 'devices':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        anchor = '\t/* port: on a co-op client only the host moves devices (network_coop.c) */'
        text = trace.once(text, anchor, '\tm0_emit("group_desired", game_time_get(), group_index, desired_value, network_coop_devices_remote(), 0, 0);\n' + anchor)
        anchor = '\tif (snap)\n\t\tdevice_group_set_actual_value(group_index, value);'
        return trace.once(text, anchor, '\tm0_emit("group_received", game_time_get(), group_index, value, flags, snap, 0);\n' + anchor)
    if name == 'hs_library_external':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        anchor = '\t\ths_object_orient(object_index, cutscene_flag_index, TRUE, TRUE);'
        return trace.once(text, anchor, '\t\tm0_emit("script_teleport", game_time_get(), object_index, cutscene_flag_index, flag->position.x, flag->position.y, flag->position.z);\n' + anchor)
    if name == 'scenario':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        text = trace.once(text, '\tshort old_structure_bsp_index = NONE;',
            '\tshort old_structure_bsp_index = NONE;\n'
            '\tm0_emit("bsp_begin", global_structure_bsp_index, structure_bsp_index, 0, 0, 0, 0);')
        return trace.once(text, '\treturn result;\n}\n\nboolean scenario_test_pas(',
            '\tm0_emit("bsp_end", global_structure_bsp_index, structure_bsp_index, result, 0, 0, 0);\n'
            '\treturn result;\n}\n\nboolean scenario_test_pas(')
    text = recovery.instrument(relative, original)
    if name == 'network_test':
        text += '\nvoid m0_world_vehicle(boolean enter)\n{\n\tnetwork_test_vehicle(enter);\n}\n'
    if name == 'network_coop':
        text = trace.once(text, 'static void m0_recovery_capture(void);',
            'static void m0_world_tick(void);\nstatic void m0_recovery_capture(void);')
        text = trace.once(text, '\tm0_recovery_fixture();', '\tm0_world_tick();\n\tm0_recovery_fixture();')
        text = trace.once(text, '\tclient_play_presentation();', '\tclient_play_presentation();\n\tm0_world_tick();')
        text += '\n' + Path(__file__).with_suffix('.c').read_text(encoding='utf-8')
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    paths = ['port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
        'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
        'port/linux/game/network_test.c', 'source/interface/hud_messaging.c',
        'source/interface/hud_nav_points.c', 'source/scenario/scenario.c',
        'source/game/players.c', 'source/devices/devices.c', 'source/hs/hs_library_external.c']
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(), 'files': {}}
    for relative in paths:
        original = (trace.ROOT / relative).read_bytes()
        target = OUTPUT / Path(relative).name
        target.write_text(instrument(relative, original.decode('utf-8').replace('\r\n', '\n')), encoding='utf-8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUTPUT / 'compile.cmd').write_text(f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}" compile %*\n')


if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] != 'compile':
            raise SystemExit('Expected compile or no arguments')
        sys.exit(trace.compile_source(sys.argv[2:]))
    generate()
