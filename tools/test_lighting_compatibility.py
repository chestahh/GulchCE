"""Do not confuse HSC player ordering or requested events with observed coverage."""
from copy import deepcopy
import pytest
from lighting_compatibility_run import report_passed


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
