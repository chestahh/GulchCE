"""Generate private M0 pool-pressure, dependency and epoch experiments."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import coop_m0_recovery_faults as faults
import coop_m0_trace as trace

OUTPUT = trace.ROOT / 'build/m0/recovery-stress/generated'
trace.OUTPUT = OUTPUT


def fixture(name):
    return Path(__file__).with_name('coop_m0_stress_' + name + '.c').read_text()


def instrument(relative, original):
    name = Path(relative).stem
    if name == 'objects':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        text = trace.once(text, 'static long object_header_new(',
            '/* port: private M0 pressure probe, generated copy only. */\n'
            'static void m0_pressure_before(long index);\nvoid m0_pressure_release(void);\n'
            'static long object_header_new(')
        text = trace.once(text, '\t\tif (memory_pool_block_allocate(object_memory_pool, (void**)&header->datum, size))',
            '\t\tm0_pressure_before(index);\n\t\tif (memory_pool_block_allocate(object_memory_pool, (void**)&header->datum, size))')
        text = trace.once(text, '\t\t\tdatum_delete(data, index);\n\t\t\tindex = NONE;',
            '\t\t\tm0_emit("pool_failed", game_time_get(), index, size, memory_pool_get_free_size(object_memory_pool), 0, 0);\n'
            '\t\t\tdatum_delete(data, index);\n\t\t\tindex = NONE;')
        # Private runs use one map. Also release retained blocks before an
        # unexpected unload, while the pool and its references are still valid.
        text = trace.once(text, 'void objects_dispose_from_old_map(\n\tvoid)\n{',
            'void objects_dispose_from_old_map(\n\tvoid)\n{\n\tm0_pressure_release();')
        return text + '\n' + fixture('pool')
    text = faults.instrument(relative, original)
    if name == 'win32_net':
        helper = 'void m0_target_write' + text.split('void m0_target_write', 1)[1]
        text += helper.replace('m0_target_', 'm0_other_').replace('m0-target.bin', 'm0-other.bin')
    elif name == 'network_coop':
        text = trace.once(text, Path(__file__).with_name('coop_m0_recovery_faults.c').read_text(), fixture('host'))
        text = 'static int m0_stress_pin_requested, m0_stress_pin_active;\n' + text
        text = trace.once(text, '\tm0_emit("checkpoint_saved", game_time_get(), coop_host(), 0, 0, 0, 0);',
            '\tm0_emit("checkpoint_saved", game_time_get(), coop_host(), 0, 0, 0, 0);\n'
            '\tif (m0_stress_pin_requested)\n\t{\n\t\tm0_stress_pin_active = 1;\n'
            '\t\tm0_stress_pin_requested = 0;\n\t\tm0_emit("checkpoint_pin", game_time_get(), 1, 0, 0, 0, 0);\n\t}')
    elif name == 'network_objects':
        text = trace.once(text, Path(__file__).with_name('coop_m0_faults_client.c').read_text(), fixture('client'))
        text = trace.once(text, '\t\tdistributed_send(&message, _distributed_message_client_ready,',
            '\t\tm0_emit("retry_request", game_time_get(), m0_fault_epoch(), objects_client_ready_interval, objects_client_retry_ticks, objects_client_failed_time, objects_client_ask_again);\n'
            '\t\tdistributed_send(&message, _distributed_message_client_ready,')
        text = trace.once(text, '\tshort index;\n\n\tmcc_network_new_game();',
            '\tshort index;\n\n\tm0_emit("reset_before", game_time_get(), m0_fault_epoch(), objects_client_retried_inventory_count, objects_client_retry_ticks, objects_client_failed_time, objects_client_ready_interval);\n\tmcc_network_new_game();')
        text = trace.once(text, '\tobjects_client_retried_inventory_count = 0;',
            '\tobjects_client_retried_inventory_count = 0;\n\tm0_emit("reset_after", game_time_get(), m0_fault_epoch(), objects_client_retried_inventory_count, objects_client_retry_ticks, objects_client_failed_time, objects_client_ready_interval);')
        text = 'extern long m0_fault_epoch(void);\n' + text
    elif name == 'network_distributed':
        text = trace.once(text, 'void network_distributed_handle_message(\n',
            'static void m0_stale_remember(void const *, word, struct distributed_message_header const *);\n'
            'void network_distributed_handle_message(\n')
        text = trace.once(text, '\tif (!distributed_message_epoch_valid(machine_index, &header, size))\n\t\treturn;',
            '\tm0_stale_remember(message, size, &header);\n'
            '\tif (!distributed_message_epoch_valid(machine_index, &header, size))\n\t{\n'
            '\t\tif (header.checkpoint_epoch == 4 && distributed_checkpoint_epoch == 5)\n'
            '\t\t\tm0_emit("old_epoch_rejected", game_time_get(), header.checkpoint_epoch, distributed_checkpoint_epoch, header.type, size, header.count);\n'
            '\t\treturn;\n\t}')
        text += '\n' + fixture('packets')
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    paths = ['port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
             'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
             'port/linux/game/network_test.c', 'source/interface/hud_messaging.c',
             'source/interface/hud_nav_points.c', 'source/objects/objects.c']
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(), 'files': {}}
    for relative in paths:
        source = trace.ROOT / relative
        original = source.read_bytes()
        target = OUTPUT / source.name
        target.write_text(instrument(relative, original.decode().replace('\r\n', '\n')))
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
