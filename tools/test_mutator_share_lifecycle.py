#!/usr/bin/env python3
"""Execute the real host fetch predicates/resets and session lifecycle functions."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = re.search(r'^(?:static )?(?:void|boolean|int) ' + name + r'\([^;{}]*\)\s*\{', source, re.M).start()
    return source[start:source.index('\n}', start) + 2]


def test_mutator_share_lifecycle():
    share = (ROOT / 'port/linux/game/mutator_share.c').read_text()
    protocol = (ROOT / 'port/linux/game/mutator_protocol.c').read_text()
    code = r'''
#include <assert.h>
#include <limits.h>
typedef int boolean;
#define FALSE 0
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 4
#define FETCHING_TIMEOUT 20000
#define MUTATOR_PROTOCOL_VERSION 1
#define MUTATOR_CAMERA_PROTOCOL_VERSION 2
static struct { unsigned long machine_asked_at[HALO_PORT_MAXIMUM_NETWORK_MACHINES]; } mutator_share;
static int mutator_host, mutator_locked;
static unsigned int mutator_host_version, mutator_client_version;
static unsigned long now = 1000;
static int enabled = 1;
static unsigned long system_milliseconds(void) { return now; }
static int config_boolean(char const *name) { (void)name; return enabled; }
static int port_mutators_selected(void) { return 0; }
static int third_person_session_active(void) { return 0; }
static void third_person_session_lock(void) {}
'''
    for source, name in (
        (protocol, 'mutator_session_active'),
        (share, 'mutator_share_machine_reset'),
        (share, 'mutator_share_host_reset'),
        (share, 'mutator_share_machine_fetching'),
        (protocol, 'mutator_session_host_begin'),
        (protocol, 'mutator_session_host_end'),
    ):
        code += '\n' + function(source, name) + '\n'
    code += r'''
int main(void) {
    int i;
    mutator_session_host_begin();
    mutator_share.machine_asked_at[1] = now;
    assert(mutator_share_machine_fetching(1));
    mutator_host_version = 0;
    assert(!mutator_share_machine_fetching(1)); /* timestamp is still recent */
    mutator_host_version = MUTATOR_PROTOCOL_VERSION;
    assert(mutator_share_machine_fetching(1));
    mutator_share.machine_asked_at[2] = now;
    mutator_share_machine_reset(1); /* departure/reuse must not clear a peer */
    assert(!mutator_share_machine_fetching(1));
    assert(mutator_share_machine_fetching(2));
    assert(!mutator_share_machine_fetching(-1));
    assert(!mutator_share_machine_fetching(HALO_PORT_MAXIMUM_NETWORK_MACHINES));
    assert(!mutator_share_machine_fetching(LONG_MAX));
    mutator_share_machine_reset(-1);
    mutator_share_machine_reset(HALO_PORT_MAXIMUM_NETWORK_MACHINES);
    mutator_share_machine_reset(LONG_MAX);
    assert(mutator_share_machine_fetching(2));
    now += FETCHING_TIMEOUT;
    assert(!mutator_share_machine_fetching(2));
    for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_MACHINES; i++)
        mutator_share.machine_asked_at[i] = now;
    mutator_session_host_end();
    for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_MACHINES; i++) {
        assert(!mutator_share.machine_asked_at[i]);
        assert(!mutator_share_machine_fetching(i));
        mutator_share.machine_asked_at[i] = now;
    }
    mutator_session_host_begin();
    assert(mutator_session_active());
    for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_MACHINES; i++) {
        assert(!mutator_share_machine_fetching(i));
        mutator_share.machine_asked_at[i] = now;
    }
    enabled = 0;
    mutator_session_host_begin(); /* even a stock session clears old host state */
    for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_MACHINES; i++)
        assert(!mutator_share.machine_asked_at[i]);
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder)
        (path / 'lifecycle.c').write_text(code)
        executable = path / 'lifecycle'
        subprocess.run([os.environ.get('CC', 'clang'), '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(path / 'lifecycle.c'), '-o', str(executable)], check=True)
        fd = os.open(executable, os.O_RDONLY)
        try:
            os.fchmod(fd, 0o755)
        finally:
            os.close(fd)
        subprocess.run([str(executable)], check=True)


def test_server_reset_hooks():
    server = (ROOT / 'source/networking/network_server_manager.c').read_text()
    for name, reset in (
        ('network_game_server_add_new_client', 'mutator_share_machine_reset(i);'),
        ('network_game_server_accept_client_machine_into_game', 'mutator_share_machine_reset(machine_index);'),
        ('network_game_server_remove_client_machine_from_game', 'mutator_share_machine_reset(client->machine_index);'),
        ('network_game_server_remove_machine_from_game', 'mutator_share_machine_reset(machine->machine_index);'),
    ):
        assert reset in function(server, name), name


if __name__ == '__main__':
    test_mutator_share_lifecycle()
    test_server_reset_hooks()
    print('mutator share lifecycle: PASS')
