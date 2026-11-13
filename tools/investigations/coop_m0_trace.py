"""Generate Windows-only observational source copies for an M0 diagnostic build.

Production files are read only. Exact anchors fail closed on source drift.
Use the generated compiler launcher with configure.py --compiler-launcher.
The copies record metadata, not packet payloads. They must not be shipped.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'build/m0/trace'

HEADER = r'''
/* M0 investigation only: observation in a generated source copy. */
#include <stdio.h>
extern double coop_m0_clock(void);
static void m0_emit(char const *kind, double a, double b, double c,
    double d, double e, double f)
{
    static FILE *file;
    if (!file) file = fopen("m0-TRACE_NAME.csv", "w");
    if (!file) return;
    fprintf(file, "%.6f,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
        coop_m0_clock(), kind, a, b, c, d, e, f);
    fflush(file);
}
'''

SOCKET_HELPER = r'''
double coop_m0_clock(void)
{
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}
static void m0_socket(char const *kind, int socket, int result, void const *address)
{
    static SRWLOCK lock = SRWLOCK_INIT;
    struct sockaddr_in peer;
    int size = sizeof(peer);
    DWORD saved_error = GetLastError();
    if (result <= 0) return;
    memset(&peer, 0, sizeof(peer));
    if (address && ((struct sockaddr const *)address)->sa_family == AF_INET)
        memcpy(&peer, address, sizeof(peer));
    else
        getpeername((SOCKET)socket, (struct sockaddr *)&peer, &size);
    AcquireSRWLockExclusive(&lock);
    m0_emit(kind, socket, result, ntohl(peer.sin_addr.s_addr), ntohs(peer.sin_port), 0, 0);
    ReleaseSRWLockExclusive(&lock);
    SetLastError(saved_error);
}
'''


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Expected one anchor, found {text.count(old)}: {old[:100]!r}')
    return text.replace(old, new)


def instrument(path, text):
    name = Path(path).stem
    if name == 'win32_net':
        text = once(text, '#include "posix.h"', '#include "posix.h"\n' +
                    HEADER.replace('TRACE_NAME', name) + SOCKET_HELPER)
        for kind, arguments in (
            ('send', '(SOCKET)socket, buffer, length, flags'),
            ('sendto', '(SOCKET)socket, buffer, length, flags, address, address_length'),
            ('recv', '(SOCKET)socket, buffer, length, flags'),
            ('recvfrom', '(SOCKET)socket, buffer, length, flags, address, address_length')):
            old = f'\treturn succeed({kind}({arguments}));'
            address = 'address' if kind.endswith(('to', 'from')) else 'NULL'
            text = once(text, old, f'\tint result = succeed({kind}({arguments}));\n'
                        f'\tm0_socket("{kind}", socket, result, {address});\n\treturn result;')
        return text
    if name == 'network_test':
        # Fixture setup uses the same public profile API as the normal menu.
        # The optional second start presses the existing start operation once;
        # it never chooses a map, changes a variant or repairs session state.
        old = '\tif (!name || !name[0])\n\t\treturn;'
        text = once(text, old, '''\tif (getenv("M0_ACTIVATE_PROFILE"))
\t{
\t\tlong profile_index = player_ui_get_player1_last_used_profile_index();
\t\tboolean loaded;
\t\tif (getenv("M0_FRESH_PROFILE")) profile_index = player_profile_new(0, L"M0 Baseline");
\t\tloaded = profile_index != NONE && player_profile_get(profile_index, &profile);
\t\tif (loaded) player_ui_set_active_player_profile(0, profile_index, &profile);
\t\tplatform_log("M0 fixture: profile index %ld loaded %d", profile_index, loaded);
\t}
''' + old)
        old = '\t/* the next game of the list: once the scores have been shown a while,'
        text = once(text, old, '''\t/* M0 fixture only: one explicit lobby Start operation after gameplay. */
\tif (getenv("M0_START_NEXT_LOBBY") && network_test.mode == _network_test_host)
\t{
\t\tstatic boolean played, requested;
\t\tstatic real lobby_seconds;
\t\tif (!main_menu_loaded && network_test.started && !played)
\t\t{
\t\t\tplayed = TRUE;
\t\t\tplatform_log("M0 fixture: current solo level during network game %d", main_get_current_solo_level());
\t\t}
\t\tif (played && main_menu_loaded && !requested)
\t\t{
\t\t\tlobby_seconds += seconds;
\t\t\tif (lobby_seconds >= 10.0f && global_network_game_client_get())
\t\t\t{
\t\t\t\trequested = TRUE;
\t\t\t\tnetwork_game_client_request_immediate_start();
\t\t\t\tplatform_log("M0 fixture: requested next lobby Start without changing selection");
\t\t\t}
\t\t}
\t}

''' + old)
        return text
    text = HEADER.replace('TRACE_NAME', name) + text
    if name == 'network_objects':
        old = '\tdistributed_checkpoint_to_machine(machine_index);\n\tplayer_list ='
        text = once(text, old, '\tm0_emit("snapshot_begin", game_time_get(), machine_index, again, 0, 0, 0);\n' + old)
        old = '\t\t\t(word)sizeof(message.header));\n\t\treturn;\n\t}\n\tdistributed_host_update_objects();'
        text = once(text, old, '\t\t\t(word)sizeof(message.header));\n'
                    '\t\tm0_emit("snapshot_cached", game_time_get(), machine_index, 0, 0, 0, 0);\n'
                    '\t\treturn;\n\t}\n\tdistributed_host_update_objects();')
        old = '\tdistributed_host_send_inventories_to_machine(machine_index);'
        text = once(text, old, old + '\n\tm0_emit("snapshot_built", game_time_get(), machine_index, objects_host_told_count, 0, 0, 0);')
        old = '\tobjects_client_synchronized = TRUE;'
        text = once(text, old, old + '\n\tm0_emit("snapshot_usable", game_time_get(), objects_statistics.create_failures, objects_client_retried_inventory_count, 0, 0, 0);')
        old = '\n\t\tobjects_client_ready_time = game_time_get();'
        # This assignment is also used by recovery; observe each real request site.
        count = text.count(old)
        if count != 1:
            raise ValueError(f'Unexpected request anchors: {count}')
        text = text.replace(old, old + '\n\tm0_emit("snapshot_request", game_time_get(), objects_client_ask_again, 0, 0, 0, 0);')
        old = '\tretried->time = game_time_get();'
        text = once(text, old, old + '\n\tm0_emit("inventory_queue", game_time_get(), objects_client_retried_inventory_count, inventory->unit_index, 0, 0, 0);')
        old = '\tdistributed_client_apply_retried_inventories();'
        text = once(text, old, old + '\n\tm0_emit("client_tick", game_time_get(), objects_client_synchronized, distributed_checkpoint_ready(), objects_client_retried_inventory_count, objects_statistics.create_failures, objects_client_new_object_count);')
        # Both hard and blended writes already retain the pre-write position.
        for indent in ('\t', '\t\t'):
            old = '\n' + indent + 'network_objects_vehicle_written(object_index, &before, &before_forward, &before_up, kind);'
            text = once(text, old, old + '\n' + indent +
                        'm0_emit("correction", game_time_get(), object_index, kind, '
                        'object->object.position.x - before.x, object->object.position.y - before.y, '
                        'object->object.position.z - before.z);')
    elif name == 'network_distributed':
        old = '\tnetwork_distributed_server_send_to_machine_reliably(machine_index, message, size);'
        text = once(text, old, '\tm0_emit("reliable_enqueue", game_time_get(), machine_index, type, size, distributed_checkpoint_epoch, count);\n' + old)
        old = '\t\t\tdistributed_checkpoint_synchronized = TRUE;'
        text = once(text, old, old + '\n\t\t\tm0_emit("snapshot_marker", game_time_get(), distributed_checkpoint_epoch, 0, 0, 0, 0);')
    elif name == 'network_coop':
        old = '\tcoop_events.count++;'
        text = once(text, old, old + '\n\tm0_emit("event_queue", game_time_get(), coop_events.count, kind, event->number, 0, 0);')
        old = '\t\terror(_error_silent, "co-op: event queue full; an event of kind %d is not sent", kind);'
        text = once(text, old, '\t\tm0_emit("event_overflow", game_time_get(), coop_events.count, kind, 0, 0, 0);\n' + old)
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), 'files': {}}
    for relative in ('port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
                     'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
                     'port/linux/game/network_test.c'):
        source = ROOT / relative
        original = source.read_bytes()
        generated = instrument(relative, original.decode('utf8').replace('\r\n', '\n'))
        target = OUTPUT / source.name
        target.write_text(generated, encoding='utf8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    launcher = f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}" compile %*\n'
    (OUTPUT / 'compile.cmd').write_text(launcher)
    print(OUTPUT / 'compile.cmd')


def compile_source(arguments):
    manifest = json.loads((OUTPUT / 'manifest.json').read_text())
    for index, argument in enumerate(arguments):
        path = Path(argument)
        if path.suffix != '.c':
            continue
        absolute = path.resolve()
        if not absolute.is_relative_to(ROOT):
            continue
        relative = absolute.relative_to(ROOT).as_posix()
        if relative in manifest['files']:
            entry = manifest['files'][relative]
            if hashlib.sha256(absolute.read_bytes()).hexdigest() != entry['source_sha256']:
                raise RuntimeError(f'Source drift after generation: {relative}')
            if hashlib.sha256(Path(entry['copy']).read_bytes()).hexdigest() != entry['copy_sha256']:
                raise RuntimeError(f'Generated copy drift after generation: {relative}')
            arguments[index] = entry['copy']
            arguments += ['-I', str(absolute.parent)]
            break
    return subprocess.call(arguments)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'compile':
        sys.exit(compile_source(sys.argv[2:]))
    argparse.ArgumentParser(description=__doc__).parse_args()
    generate()
