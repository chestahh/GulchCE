"""Bounded dependency queues through unchanged production functions."""
import pytest

from harness import build, constant, mutated, read, run
from test_coop_checkpoint_recovery import generated


def queue_world():
    capacity = constant(read('port/linux/game/network_distributed.h') +
        read('port/linux/include/halo_port_limits.h'), 'MAXIMUM_TRACKED_PLAYERS')
    world = mutated(read('tools/harness/tests/coop_checkpoint_recovery.c'),
        '#define MAXIMUM_TRACKED_PLAYERS 2', f'#define MAXIMUM_TRACKED_PLAYERS {capacity}')
    world = mutated(world, '#define MAXIMUM_TRACKED_OBJECTS 128',
        f'#define MAXIMUM_TRACKED_OBJECTS {capacity + 2}')
    world = mutated(world, 'static int world_capacity=16;',
        f'static int world_capacity={capacity + 2};')
    return ('stress_world.inc', world)


@pytest.mark.parametrize('case', ['capacity', 'expiry', 'refresh'])
def test_recovery_queue(case):
    status, output = run(build('coop_recovery_queue', generated() + (queue_world(),)), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', [
    ('game_time_get() - retried->time >= EMPTY_INVENTORY_TICKS',
     'game_time_get() - retried->time > EMPTY_INVENTORY_TICKS', 'expiry'),
    ('objects_client_retried_inventories[index].time < objects_client_retried_inventories[oldest].time',
     'objects_client_retried_inventories[index].time > objects_client_retried_inventories[oldest].time', 'capacity'),
    ('retried->inventory = *inventory;', 'if (retried->time == 0) retried->inventory = *inventory;', 'refresh'),
])
def test_queue_checks_detect_broken_lifetime_or_replacement(before, after, case):
    status, output = run(build('coop_recovery_queue', generated((before, after)) + (queue_world(),)), case)
    assert status == 1, output
