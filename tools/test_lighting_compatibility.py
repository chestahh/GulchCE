"""Do not confuse HSC player ordering or requested events with observed coverage."""
from copy import deepcopy
import csv
import pytest
from lighting_compatibility_run import CYCLE_SCENARIOS, cycle_rounds, report_passed, policy_samples


def report(player):
    peer=dict(exit=0,forced_stop=False,two_player_samples=30,respawns={str(player):2},
              inventories={'0':[['ar'],['ar','plasma']]},policy_samples=20,
              correct_policy_samples=20,config_verified=True,failures=[],
              one_player_samples=20,first_two_player_tick=700)
    return dict(case='xbox',peers={'host':peer,'client':deepcopy(peer)},
                agreement=dict(problems=[],divergences=[],figures={'seconds':25}))


@pytest.mark.parametrize('player',[0,1])
def test_either_network_player_can_be_the_hsc_target(player):
    assert report_passed(report(player))


@pytest.mark.parametrize('fault',['no_respawn','different_player','policy','divergence','not_late','no_pairs'])
def test_missing_coverage_is_not_a_pass(fault):
    r=report(0)
    if fault=='no_respawn':r['peers']['client']['respawns']={'0':0}
    if fault=='different_player':r['peers']['client']['respawns']={'1':2}
    if fault=='policy':r['peers']['client']['correct_policy_samples']=19
    if fault=='divergence':r['agreement']['divergences']=['weapon mismatch']
    if fault=='not_late':r['case']='late';r['peers']['host']['one_player_samples']=0
    if fault=='no_pairs':r['agreement']['figures']['seconds']=0
    assert not report_passed(r)


@pytest.mark.parametrize('counts,exit_code,passed', [
    ([1,2,1,2,1],0,True), ([1,2],0,False),
    ([1,2,1],0,False), ([1,2,1,2],1,False),
])
def test_reconnect_requires_observed_leave_and_return(counts,exit_code,passed):
    r=report(0)
    r['reconnect']=dict(host_player_counts=counts,client_first_exit=exit_code)
    assert report_passed(r)==passed


def test_late_policy_checks_receipt_instead_of_global_game_age():
    pending=dict(tick=900,connection=1,policy_received=0,policy=0,shield_enabled=0)
    applied=dict(tick=930,connection=1,policy_received=1,policy=-1,shield_enabled=1)
    received,correct,waiting,safe=policy_samples([pending,applied],1,-1,1)
    assert received==correct==[applied] and waiting==[pending] and safe
    pending['shield_enabled']=1
    assert not policy_samples([pending,applied],1,-1,1)[3]
    applied['policy']=0
    assert not policy_samples([applied],1,-1,1)[1]
    assert not policy_samples([pending],1,-1,1)[0]


@pytest.mark.parametrize('fault', [None, 'wrong_level', 'missing_sun', 'wrong_mode', 'early_policy', 'no_peer'])
def test_family_cycle_requires_observed_maps_and_render_state(tmp_path, fault):
    path = tmp_path / 'shadow-state.csv'
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=(
            'family', 'scenario', 'connection', 'players', 'mode', 'maps', 'sun', 'spots',
            'received', 'policy', 'shield'))
        writer.writeheader()
        for index, (family, scenario) in enumerate(CYCLE_SCENARIOS):
            for sample in range(9):
                row = dict(family=family, scenario=scenario, connection=1, players=2,
                           mode=2, maps=1, sun=int(index>0), spots=int(index==0), received=1, policy=4, shield=1)
                if index == 2:
                    if fault == 'wrong_level': row['scenario'] = 'another MCC map'
                    if fault == 'missing_sun': row['sun'] = 0
                    if fault == 'wrong_mode': row['mode'] = 1
                    if fault == 'early_policy' and sample == 0: row['received'] = 0
                    if fault == 'no_peer': row['players'] = 1
                writer.writerow(row)
    failures = []
    cycle_rounds(path, 1, 2, failures)
    assert bool(failures) == bool(fault)

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
