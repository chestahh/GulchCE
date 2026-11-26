"""Rendered, isolated two-peer Windows lighting compatibility scenarios.

Uses the engine's existing join, damage, pickup and respawn test controls.
Retail maps stay local; the runner removes staged maps and archives test saves.
"""
import argparse
import csv
import hashlib
import json
import os
from itertools import groupby
from pathlib import Path
import re
import shutil
import subprocess
import time
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from lighting_performance_run import cleanup, LIGHTS
from net_compare import parse_log, judge_run, TOLERANCES

ROOT=Path(__file__).resolve().parents[1]
CASES={
    'xbox': ('bloodgulch',1,'glow_overshield',1),
    'ce': ('custom_maps\\deathisland',0,'stock',1),
    'mcc': ('mcc_maps\\DigsiteMP\\1',1,'players',1),
    'cycle': ('ratrace', 1, 'glow_overshield', 1),
    'late': ('bloodgulch',0,'players',32),
}
CYCLE = ('ratrace', CASES['ce'][0], CASES['mcc'][0], 'bloodgulch')
# These are the tags each family actually loaded. CE resolves its public
# custom_maps name to the authored scenario inside the cache.
CYCLE_SCENARIOS = (
    ('Xbox', r'levels\test\ratrace\ratrace'),
    ('CE', r'levels\test\deathisland\deathisland'),
    ('MCC', CASES['mcc'][0]),
    ('Xbox', r'levels\test\bloodgulch\bloodgulch'),
)

# A combat/death cue can remove the unit before the scripted cue executes.
# The health query safely returns -1 for NONE; unit_kill requires a live unit.
DEATH_CUE = 'if (> (unit_get_health (unit (list_get (players) 1))) 0) (unit_kill (unit (list_get (players) 1)))'


def policy_samples(rows,connection,expected_policy,expected_shield):
    # The client deliberately draws stock until the validated reliable policy
    # message arrives. Global game tick is not the age of a late connection.
    connected=[r for r in rows if r['connection']==connection]
    received=[r for r in connected if r['policy_received']]
    pending=[r for r in connected if not r['policy_received']]
    correct=[r for r in received if r['policy']==expected_policy and r['shield_enabled']==expected_shield]
    safe=all(r['policy']==0 and r['shield_enabled']==0 for r in pending)
    return received,correct,pending,safe


def report_passed(result):
    peers=result['peers']
    if result.get('reconnect'):
        reconnect=result['reconnect']
        counts=reconnect['host_player_counts']
        if reconnect['client_first_exit'] != 0 or not any(counts[i:i+3]==[2,1,2] for i in range(len(counts)-2)):
            return False
    if set(peers)!= {'host','client'}:return False
    for peer in peers.values():
        if not (peer['exit']==0 and not peer['forced_stop'] and peer['two_player_samples']>10 and
                sum(peer['respawns'].values())>0 and any(len(v)>1 for v in peer['inventories'].values()) and
                peer['policy_samples']>5 and peer['correct_policy_samples']==peer['policy_samples'] and
                peer['config_verified'] and not peer['failures']):return False
    # HSC's players-list order is not the network log's player index order.
    # Require both peers to observe a respawn of the same network player.
    respawned=[{str(k) for k,v in p['respawns'].items() if v>0} for p in peers.values()]
    if not respawned[0].intersection(respawned[1]):return False
    if result['agreement']['problems'] or result['agreement']['divergences']:return False
    if result['case']=='late':
        # Late join can phase-shift the once-a-second snapshots beyond the
        # comparator's two-tick window. This case covers joining/lifecycle and
        # host-measured synchronous drift, not paired snapshot equivalence.
        return peers['host']['one_player_samples']>5 and (peers['host']['first_two_player_tick'] or 0)>300
    return result['agreement']['figures']['seconds']>10


def cycle_rounds(path, connection, mode, failures):
    rounds = []
    if not path.exists():
        failures.append('Missing rendered shadow-state samples')
        return rounds
    with path.open() as stream:
        for (family, scenario), group in groupby(csv.DictReader(stream),
                key=lambda row: (row['family'], row['scenario'])):
            rows = list(group)
            both = [r for r in rows if int(r['connection']) == connection and int(r['players']) == 2]
            applied = [r for r in both if int(r['received'])]
            pending = [r for r in both if not int(r['received'])]
            active = sum(bool(int(r['maps'])) for r in both)
            sun = sum(bool(int(r['sun'])) for r in both)
            fixtures = sum(bool(int(r['spots']) & 3) for r in both)
            rounds.append(dict(family=family, scenario=scenario, two_player_samples=len(both),
                               applied_policy_samples=len(applied), pending_policy_samples=len(pending),
                               local_map_samples=active, sun_samples=sun, fixture_samples=fixtures))
            if len(applied) < 8 or any(int(r['policy']) != 4 or int(r['shield']) != 1 for r in applied):
                failures.append(f'{scenario}: missing or incorrect applied host policy')
            if any(int(r['policy']) or int(r['shield']) for r in pending):
                failures.append(f'{scenario}: effects enabled before policy receipt')
            if any(int(r['mode']) != mode for r in rows) or bool(active) != bool(mode):
                failures.append(f'{scenario}: shadow mode or local map activity mismatch')
            if mode == 0 and any(int(r[k]) for r in rows for k in ('maps', 'sun', 'spots')):
                failures.append(f'{scenario}: Classic rendered a modern shadow')
    if tuple((r['family'], r['scenario']) for r in rounds) != CYCLE_SCENARIOS:
        failures.append('Wrong loaded scenario or family order')
    elif mode:
        if not rounds[0]['fixture_samples'] or rounds[0]['sun_samples']:
            failures.append('Rat Race must exercise indoor fixtures without a sun map')
        if any(not r['sun_samples'] for r in rounds[1:]):
            failures.append('Missing outdoor sun after changing maps')
    return rounds


def cycle_agreement(base):
    # net_compare reads the last game in a log. Split when the simulation tick
    # restarts so a good final round cannot hide a divergence in an earlier one.
    agreements = []
    with TemporaryDirectory(dir=base) as temp:
        files = {}
        for role in ('host', 'client'):
            games = []
            last_tick = -1
            with (base/role/'stderr.log').open(errors='replace') as log:
                for line in log:
                    marker = re.search(r'network test: tick (\d+) player', line)
                    if not marker:
                        continue
                    tick = int(marker[1])
                    if not games or tick < last_tick:
                        games.append([])
                    games[-1].append(line)
                    last_tick = tick
            if len(games) != len(CYCLE):
                return []
            files[role] = []
            for index, lines in enumerate(games):
                path = Path(temp) / f'{role}-{index}.log'
                path.write_text(''.join(lines))
                files[role].append(path)
        for host, client in zip(files['host'], files['client']):
            agreement = judge_run(host, client, SimpleNamespace(seconds=30, skip=5), TOLERANCES)
            respawns = []
            for path in (host, client):
                data, _, _ = parse_log(path)
                observed = set()
                for player in (0, 1):
                    states = [s['players'][player]['alive'] for s in data.values() if player in s['players']]
                    changes = [alive for alive, _ in groupby(states)]
                    if any(changes[i:i+3] == [True, False, True] for i in range(len(changes)-2)):
                        observed.add(player)
                respawns.append(observed)
            agreement['confirmed_respawns'] = sorted(respawns[0] & respawns[1])
            agreements.append(agreement)
    return agreements


def cycle_report(base, children, forced, settings):
    """Require both running peers to render all four rounds in order."""
    result = {'case': 'cycle', 'levels': CYCLE, 'peers': {}}
    for role, process in children.items():
        folder = base / role
        failures = []
        mode = ('off', 'hard', 'soft').index(settings[role]['modern_shadows'])
        connection = 2 if role == 'host' else 1
        log = (folder/'stderr.log').read_text(errors='replace')
        if process.returncode or forced[role]:
            failures.append('Process did not exit normally')
        if role == 'client' and log.count('network test: joining\n') != 1:
            failures.append('Expected a single join across the rotation')
        if role == 'host' and log.count('network test: the next game') != len(CYCLE) - 1:
            failures.append('Expected three returns to the ordinary lobby')
        failures.extend(re.findall(r'(?im)^.*(?:crash: exception|exception halt|assertion failed|shader.*(?:error|failed)|GL_INVALID).*$', log))
        rounds = cycle_rounds(folder/'shadow-state.csv', connection, mode, failures)
        result['peers'][role] = dict(rounds=rounds, failures=failures, exit=process.returncode)
    result['agreement'] = cycle_agreement(base)
    result['passed'] = (set(result['peers']) == {'host', 'client'} and
                        all(not peer['failures'] for peer in result['peers'].values()) and
                        len(result['agreement']) == len(CYCLE) and
                        all(not r['problems'] and not r['divergences'] and r['confirmed_respawns'] and
                            r['figures']['seconds'] > 10 for r in result['agreement']))
    (base/'report.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2), flush=True)
    return 0 if result['passed'] else 1


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root',type=Path,required=True)
    parser.add_argument('--case',choices=CASES,required=True)
    parser.add_argument('--label',help='Unique evidence folder for a revised scenario')
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build/lighting-compatibility-build')
    parser.add_argument('--host-shadows',choices=['off','hard','soft'],default='off')
    parser.add_argument('--client-shadows',choices=['off','hard','soft'],default='off')
    parser.add_argument('--mcc-package',type=Path)
    parser.add_argument('--fire',action='store_true',help='Use real trigger input as well as the scripted damage/respawn probes')
    parser.add_argument('--reconnect',action='store_true',help='Restart the client with its saved settings while the host keeps running')
    parser.add_argument('--pickup-at',type=int,default=12,help='Game seconds before the forced weapon-pickup setup')
    args=parser.parse_args()
    if args.reconnect and args.case in ('late', 'cycle'):
        parser.error('Run reconnect as a separate scenario')
    level,host_enabled,policy,delay=CASES[args.case]
    label=args.label or args.case
    assert re.fullmatch(r'[A-Za-z0-9_-]+',label)
    base=(ROOT/'build/lighting-compatibility'/label).resolve()
    assert base.is_relative_to((ROOT/'build').resolve())
    base.mkdir(parents=True,exist_ok=False)
    build=args.build_dir.resolve()
    manifest=json.loads((build/'manifest.json').read_text())
    assert hashlib.sha256((build/'halo-compatibility.exe').read_bytes()).hexdigest()==manifest['executable_sha256']
    assert hashlib.sha256((ROOT/'port/linux/game/plasma_lighting.c').read_bytes()).hexdigest()==manifest['source_sha256']
    copies=['maps/ui.map','maps/bloodgulch.map','maps/carousel.map']
    if args.case=='cycle': copies.append('maps/ratrace.map')
    if args.case in ('ce', 'cycle'):copies += ['custom_maps/'+n+'.map' for n in ('deathisland','bitmaps','sounds','loc')]
    if args.case in ('mcc', 'cycle'):copies += ['mcc_maps/DigsiteMP/'+n+'.map' for n in ('1','bitmaps','sounds')]
    children={};handles=[];settings={};forced={};outputs=[];first_exit=None
    # Stage both peers before starting the host; copy time must not consume its join window.
    try:
        for role in ('host','client'):
            out=base/role;out.mkdir();outputs.append(out)
            for name in copies:
                target=out/name;target.parent.mkdir(parents=True,exist_ok=True)
                source=args.data_root/name
                if args.mcc_package and name.startswith('mcc_maps/DigsiteMP/'):
                    source=args.mcc_package/('maps' if target.name=='1.map' else 'halo1/maps')/target.name
                shutil.copy2(source,target)
            (out/'screenshots').mkdir()
            shutil.copy2(build/'halo-compatibility.exe',out/'halo-performance.exe')
            shutil.copy2(ROOT/'build/windows/SDL3.dll',out/'SDL3.dll')
            shutil.copy2(build/'manifest.json',out/'build-manifest.json')
            enabled=host_enabled if role=='host' else 1-host_enabled
            display={key:bool(enabled) for key in LIGHTS}
            display.update(overshield_effect=bool(enabled),per_pixel_lighting=True,shield_glow_intensity='default',dynamic_lighting_reset=True)
            display['modern_shadows']=args.host_shadows if role=='host' else args.client_shadows
            if args.case == 'cycle':
                display.update({key:True for key in LIGHTS})
                display.update(shield_glow=(role=='host'), shadow_resolution=1024, environment_sun=True, environment_fixtures=True,
                               flashlight_shadows=True, flashlight_extra_slot=False)
            settings[role]=display
            (out/'config.toml').write_text('[display]\n'+''.join(f'{k} = {json.dumps(v)}\n' for k,v in display.items()))
        started=time.monotonic()
        for role,address,peer in (('host','127.0.0.232','127.0.0.233'),('client','127.0.0.233','127.0.0.232')):
            if role=='client':time.sleep(delay)
            out=base/role
            env={k:v for k,v in os.environ.items() if not k.startswith('HALO_')}
            env.update(HALO_DATA_ROOT=str(out),HALO_SAVE_ROOT=str(out/'saves'),HALO_HIDDEN_WINDOW='1',
                HALO_WINDOW_SIZE='1280x720',HALO_NO_AUDIO='1',HALO_MAX_FPS='60',HALO_NO_VSYNC='1',
                HALO_EXIT_AFTER=str(110 if role=='host' else 106-delay),HALO_NET_ONLINE='0',
                HALO_NET_PUBLIC_LOBBY='0',HALO_NET_ALLOW_UPNP='0',HALO_NET_JOIN_FROM_CLIPBOARD='0',
                HALO_NET_ADDRESS=address,HALO_NET_BROADCAST=peer,
                HALO_NETWORK_TEST='host:'+level if role=='host' else 'join',HALO_NETWORK_TEST_START='12',
                HALO_NETWORK_TEST_SCORE='10000',HALO_NETWORK_TEST_SHOOT='7',HALO_NETWORK_TEST_KILL='25',
                HALO_NETWORK_TEST_PICKUP=str(args.pickup_at),HALO_NETWORK_TEST_PICKUP_WEAPON='',
                HALO_NETWORK_TEST_CUE_PLAYERS='2',
                HALO_NETWORK_TEST_CUES=f'30 {DEATH_CUE}|60 {DEATH_CUE}' if role=='host' else '',
                HALO_NETWORK_TEST_NAME='LightingHost' if role=='host' else 'LightingPeer',
                HALO_TEST_INPUT='look:1' if role=='host' else 'look:2',
                HALO_NET_SHIELD_EFFECTS=policy if role=='host' else ('stock' if policy!='stock' else 'glow_overshield'),
                HALO_UPDATE_ANSWER='never',HALO_CRASH_REPORTS='no',
                HALO_SCREENSHOT_DIR=str(out/'screenshots'),HALO_SCREENSHOT_EVERY='1800')
            if args.case == 'cycle':
                env.update(HALO_EXIT_AFTER='305' if role=='host' else '301',
                           HALO_NETWORK_TEST_KILL='', HALO_NETWORK_TEST_SHOOT='', HALO_NETWORK_TEST_PICKUP='')
                if role == 'host':
                    env.update(HALO_NETWORK_TEST='host:'+level+':'+','.join(['slayer']*len(CYCLE)),
                               HALO_NETWORK_TEST_CUES=f'8 {DEATH_CUE}|37 sv_end_game')
                    for index, selected in enumerate(CYCLE[1:], 2):
                        env[f'HALO_SHADOW_TEST_MAP_{index}'] = selected
            if args.fire:env['HALO_SHADOW_COMPAT_FIRE']='1'
            if args.reconnect and role=='client':env['HALO_EXIT_AFTER']='40'
            if args.reconnect and role=='host':
                # The stock two-player match ends when the opponent leaves.
                # Use the existing next-variant control to return through the
                # real postgame/lobby flow before the client plays again.
                env.update(HALO_EXIT_AFTER='150',HALO_NETWORK_TEST='host:'+level+':slayer,slayer')
            (out/'test-settings.json').write_text(json.dumps(dict(display=settings[role],environment={k:v for k,v in env.items() if k.startswith('HALO_')}),indent=2))
            stdout=(out/'stdout.log').open('wb');stderr=(out/'stderr.log').open('wb');handles += [stdout,stderr]
            children[role]=subprocess.Popen([str(out/'halo-performance.exe')],cwd=out,env=env,
                stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW)
        if args.reconnect:
            # The last launch owns the client environment and log handles.
            # A normal exit exercises shutdown; the second process retains its
            # profile/config but must rebuild all render and session resources.
            first_exit=children['client'].wait(timeout=65)
            assert first_exit==0, 'First client failed before reconnect'
            for name in ('lighting-policy','shadow-state'):
                path=out/(name+'.csv')
                if path.exists():path.rename(out/(name+'-before-reconnect.csv'))
            stdout.close();stderr.close()
            for name in ('stdout','stderr'):
                (out/(name+'.log')).rename(out/(name+'-before-reconnect.log'))
            stdout=(out/'stdout.log').open('wb');stderr=(out/'stderr.log').open('wb');handles += [stdout,stderr]
            time.sleep(5)
            env.update(HALO_EXIT_AFTER='95',HALO_NETWORK_TEST_PICKUP='',HALO_NETWORK_TEST_KILL='',HALO_NETWORK_TEST_SHOOT='')
            (out/'reconnect-settings.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith('HALO_')},indent=2))
            children['client']=subprocess.Popen([str(out/'halo-performance.exe')],cwd=out,env=env,
                stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW)
        timeout = 340 if args.case=='cycle' else 190 if args.reconnect else 150
        while any(p.poll() is None for p in children.values()) and time.monotonic()-started < timeout:
            time.sleep(.25)
    finally:
        for role,p in children.items():
            forced[role]=p.poll() is None
            if forced[role]:p.terminate();p.wait(timeout=10)
        for handle in handles:handle.close()
        for out in outputs:cleanup(out)
    if args.case == 'cycle':
        return cycle_report(base, children, forced, settings)
    result=dict(case=args.case,level=level,policy=policy,join_delay=delay,peers={})
    if args.reconnect:result['reconnect']=dict(client_first_exit=first_exit,host_player_counts=[])
    for role,p in children.items():
        out=base/role;log=(out/'stderr.log').read_text(errors='replace')
        samples,names,errors=parse_log(out/'stderr.log')
        rows=list(samples.values());both=[r for r in rows if len(r['players'])==2]
        inventories={};respawns={}
        for player in (0,1):
            states=[r['players'][player] for r in rows if player in r['players']]
            inventories[player]=sorted(set(tuple(w[0] for w in s.get('weapons',[])) for s in states if s['alive']))
            respawns[player]=sum(not a['alive'] and b['alive'] for a,b in zip(states,states[1:]))
        policy_rows=[]
        for path in sorted(out.glob('lighting-policy*.csv')):
            with path.open() as f:policy_rows.extend({k:int(v) for k,v in r.items()} for r in csv.DictReader(f))
        if args.reconnect and role=='host':
            counts=result['reconnect']['host_player_counts']
            for row in policy_rows:
                if row['connection']!=2:continue
                count=row['active_players']
                if not counts or counts[-1]!=count:counts.append(count)
        expected_policy={'stock':0,'glow_overshield':4,'players':-1}[policy]
        expected_shield=int(settings[role]['shield_glow']) if policy=='players' else int(policy!='stock')
        stable,correct,pending,safe=policy_samples(policy_rows,2 if role=='host' else 1,expected_policy,expected_shield)
        failures=errors+re.findall(r'(?im)^.*(?:crash: exception|exception halt|assertion failed|shader.*(?:error|failed)|GL_INVALID).*$',log)
        if not safe:failures.append('Client enabled shield effects before receiving host policy')
        if not stable or any(r['local_plasma'] != int(settings[role]['plasma_lighting']) or
                             r['local_needler'] != int(settings[role]['needler_lighting']) or
                             r['local_shield'] != int(settings[role]['shield_glow']) for r in stable):
            failures.append('Runtime lighting preferences differ from the requested settings')
        debug=(out/'debug.txt').read_text(errors='replace') if (out/'debug.txt').exists() else ''
        failures += re.findall(r'(?im)^.*(?:i expected a function name|not while playing in another).*$',debug)
        config_paths=re.findall(r'^halo-linux: settings: ([^\r\n]*config\.toml)\s*$',log,re.M)
        config_ok=bool(config_paths) and all(Path(v.strip()).resolve()==(out/'config.toml').resolve() for v in config_paths)
        result['peers'][role]=dict(exit=p.returncode,forced_stop=forced[role],two_player_samples=len(both),
            one_player_samples=sum(len(r['players'])==1 for r in rows),
            first_two_player_tick=both[0]['tick'] if both else None,respawns=respawns,inventories=inventories,
            shoot_events=log.count('shoots player'),kill_events=log.count('first player kills'),
            pickup_events=log.count('stands on weapon'),policy_samples=len(stable),correct_policy_samples=len(correct),pending_policy_samples=len(pending),
            policy_values=sorted(set(r['policy'] for r in stable)),config_verified=config_ok,failures=failures)
    for role in ('host','client'):
        rows=[]
        for path in sorted((base/role).glob('shadow-state*.csv')):
            with path.open() as stream:rows.extend(csv.DictReader(stream))
        expected={'off':0,'hard':1,'soft':2}[settings[role]['modern_shadows']]
        good=bool(rows) and all(int(row['mode'])==expected and (expected or int(row['maps'])==0) for row in rows)
        result['peers'][role]['shadow_mode_correct']=good
        result['peers'][role]['shadow_active_samples']=sum(int(row['maps'])!=0 for row in rows)
        if not good:result['peers'][role]['failures'].append('Shadow setting or disabled GPU isolation mismatch')
        if expected and settings[role]['plasma_lighting'] and not result['peers'][role]['shadow_active_samples']:
            result['peers'][role]['failures'].append('Enabled shadow peer never rendered a shadow map')
    host_log=base/'host/stderr.log'
    if args.reconnect:
        log=host_log.read_text(errors='replace')
        assert 'network test: the next game' in log, 'Host never returned through the normal postgame flow'
        host_log=base/'host/reconnected-game.log'
        host_log.write_text(log.rsplit('network test: the next game',1)[1])
    result['agreement']=judge_run(host_log,base/'client/stderr.log',SimpleNamespace(seconds=45,skip=8),TOLERANCES)
    result['paired_snapshot_seconds']=result['agreement'].get('figures',{}).get('seconds',0)
    result['passed']=report_passed(result)
    (base/'report.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2),flush=True)
    return 0 if result['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())

# GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
# This grant covers chestahh's lighting, shadow and renderer changes in this file.
# Retain this credit and provenance when redistributing copies or modifications.
# Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc
