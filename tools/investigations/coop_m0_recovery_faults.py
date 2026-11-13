"""Generate isolated cross-type reuse and recovery fault experiments.

The only allocation change is one controlled refusal in a generated copy.
Production source, pool capacities and network messages remain untouched.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import coop_m0_recovery as recovery
import coop_m0_trace as trace

OUTPUT = trace.ROOT / 'build/m0/recovery-faults/generated'
trace.OUTPUT = OUTPUT


def instrument(relative, original):
    name = Path(relative).stem
    if name == 'objects':
        text = trace.once(original, 'static long object_header_new(',
            '/* port: M0 generated-only allocation fault boundary. */\n'
            'extern int m0_fault_allocation(long index);\n\nstatic long object_header_new(')
        return trace.once(text, 'if (memory_pool_block_allocate(object_memory_pool, (void**)&header->datum, size))',
            'if (!m0_fault_allocation(index) && memory_pool_block_allocate(object_memory_pool, (void**)&header->datum, size))')
    text = recovery.instrument(relative, original, identity=True)
    if name == 'win32_net':
        # Native file I/O avoids the game's data-root path normalization.
        text += r'''
void m0_target_write(long target, long unit, long definition)
{
    long values[3];
    DWORD written;
    HANDLE file = CreateFileA("../m0-target.bin", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    values[0] = target; values[1] = unit; values[2] = definition;
    WriteFile(file, values, sizeof(values), &written, NULL);
    CloseHandle(file);
}
int m0_target_read(long *target, long *unit, long *definition)
{
    long values[3];
    DWORD count = 0;
    HANDLE file = CreateFileA("../m0-target.bin", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    ReadFile(file, values, sizeof(values), &count, NULL);
    CloseHandle(file);
    if (count != sizeof(values)) return 0;
    *target = values[0]; *unit = values[1]; *definition = values[2];
    return 1;
}
'''
    elif name == 'network_coop':
        previous = Path(__file__).with_name('coop_m0_recovery_identity.c').read_text()
        helpers = previous.split('static void m0_domains_fixture(void)', 1)[0]
        fixture = Path(__file__).with_suffix('.c').read_text()
        text = trace.once(text, previous, helpers + fixture)
    elif name == 'network_distributed':
        text += '\nlong m0_fault_epoch(void) { return distributed_checkpoint_epoch; }\n'
    elif name == 'network_objects':
        text = trace.once(text, 'static void distributed_client_create(\n',
            'static boolean m0_dependency_hold(struct distributed_object_change const *change);\n'
            'static void m0_dependency_update(void);\nstatic void m0_fault_observe(void);\n'
            'static void distributed_client_create(\n')
        text = trace.once(text, '\tcheckpoint_pending = objects_client_checkpoint_pending[absolute_index];',
            '\tif (m0_dependency_hold(change)) return;\n\tcheckpoint_pending = objects_client_checkpoint_pending[absolute_index];')
        text = trace.once(text, 'void network_objects_client_tick(\n\tvoid)\n{',
            'void network_objects_client_tick(\n\tvoid)\n{\n\tm0_dependency_update();')
        text = trace.once(text, '\tif (!distributed_checkpoint_ready())\n\t\treturn;',
            '\tm0_fault_observe();\n\tif (!distributed_checkpoint_ready())\n\t\treturn;')
        text = trace.once(text, 'static void distributed_client_create_failed(void)\n{',
            'static void distributed_client_create_failed(void)\n{\n'
            '\tm0_emit("create_failed", game_time_get(), objects_client_failed_time, objects_client_retry_ticks, 0, 0, 0);')
        text += '\n' + Path(__file__).with_name('coop_m0_faults_client.c').read_text()
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(), 'files': {}}
    paths = ['port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
             'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
             'port/linux/game/network_test.c', 'source/interface/hud_messaging.c',
             'source/interface/hud_nav_points.c', 'source/objects/objects.c']
    for relative in paths:
        source = trace.ROOT / relative
        original = source.read_bytes()
        target = OUTPUT / source.name
        target.write_text(instrument(relative, original.decode().replace('\r\n', '\n')), encoding='utf8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUTPUT / 'compile.cmd').write_text(f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}" compile %*\n')
    print(OUTPUT / 'compile.cmd')


if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] != 'compile':
            raise SystemExit('Expected compile or no arguments')
        sys.exit(trace.compile_source(sys.argv[2:]))
    generate()
