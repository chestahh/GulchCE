"""Rendered, isolated two-peer Windows lighting compatibility scenarios.

Uses the engine's existing join, damage, pickup and respawn test controls.
Retail maps stay local; the runner removes staged maps and archives test saves.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from types import SimpleNamespace
from lighting_performance_run import cleanup, LIGHTS
from net_compare import parse_log, judge_run, TOLERANCES

ROOT=Path(__file__).resolve().parents[1]
CASES={
    'xbox': ('bloodgulch',1,'glow_overshield',1),
    'ce': ('custom_maps\\deathisland',0,'stock',1),
    'mcc': ('mcc_maps\\DigsiteMP\\1',1,'players',1),
    'late': ('bloodgulch',0,'players',32),
}
# A combat/death cue can remove the unit before the scripted cue executes.
# The health query safely returns -1 for NONE; unit_kill requires a live unit.
DEATH_CUE = 'if (> (unit_get_health (unit (list_get (players) 1))) 0) (unit_kill (unit (list_get (players) 1)))'


def report_passed(result):
    peers=result['peers']
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


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-root',type=Path,required=True)
    parser.add_argument('--case',choices=CASES,required=True)
    parser.add_argument('--label',help='Unique evidence folder for a revised scenario')
    args=parser.parse_args()
    level,host_enabled,policy,delay=CASES[args.case]
    label=args.label or args.case
    assert re.fullmatch(r'[A-Za-z0-9_-]+',label)
    base=(ROOT/'build/lighting-compatibility'/label).resolve()
    assert base.is_relative_to((ROOT/'build').resolve())
    base.mkdir(parents=True,exist_ok=False)
    build=ROOT/'build/lighting-compatibility-build'
    manifest=json.loads((build/'manifest.json').read_text())
    assert hashlib.sha256((build/'halo-compatibility.exe').read_bytes()).hexdigest()==manifest['executable_sha256']
    assert hashlib.sha256((ROOT/'port/linux/game/plasma_lighting.c').read_bytes()).hexdigest()==manifest['source_sha256']
    copies=['maps/ui.map','maps/bloodgulch.map','maps/carousel.map']
    if args.case=='ce':copies += ['custom_maps/'+n+'.map' for n in ('deathisland','bitmaps','sounds','loc')]
    if args.case=='mcc':copies += ['mcc_maps/DigsiteMP/'+n+'.map' for n in ('1','bitmaps','sounds')]
    children={};handles=[];settings={};forced={};outputs=[]
    # Stage both peers before starting the host; copy time must not consume its join window.
    try:
        for role in ('host','client'):
            out=base/role;out.mkdir();outputs.append(out)
            for name in copies:
                target=out/name;target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(args.data_root/name,target)
            (out/'screenshots').mkdir()
            shutil.copy2(build/'halo-compatibility.exe',out/'halo-performance.exe')
            shutil.copy2(ROOT/'build/windows/SDL3.dll',out/'SDL3.dll')
            shutil.copy2(build/'manifest.json',out/'build-manifest.json')
            enabled=host_enabled if role=='host' else 1-host_enabled
            display={key:bool(enabled) for key in LIGHTS}
            display.update(overshield_effect=bool(enabled),per_pixel_lighting=True,shield_glow_intensity='default')
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
                HALO_NETWORK_TEST_PICKUP='12',HALO_NETWORK_TEST_PICKUP_WEAPON='',
                HALO_NETWORK_TEST_CUE_PLAYERS='2',
                HALO_NETWORK_TEST_CUES=f'30 {DEATH_CUE}|60 {DEATH_CUE}' if role=='host' else '',
                HALO_NETWORK_TEST_NAME='LightingHost' if role=='host' else 'LightingPeer',
                HALO_TEST_INPUT='look:1' if role=='host' else 'look:2',
                HALO_NET_SHIELD_EFFECTS=policy if role=='host' else ('stock' if policy!='stock' else 'glow_overshield'),
                HALO_UPDATE_ANSWER='never',HALO_CRASH_REPORTS='no',
                HALO_SCREENSHOT_DIR=str(out/'screenshots'),HALO_SCREENSHOT_EVERY='1800')
            (out/'test-settings.json').write_text(json.dumps(dict(display=settings[role],environment={k:v for k,v in env.items() if k.startswith('HALO_')}),indent=2))
            stdout=(out/'stdout.log').open('wb');stderr=(out/'stderr.log').open('wb');handles += [stdout,stderr]
            children[role]=subprocess.Popen([str(out/'halo-performance.exe')],cwd=out,env=env,
                stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW)
        while any(p.poll() is None for p in children.values()) and time.monotonic()-started<150:time.sleep(.25)
    finally:
        for role,p in children.items():
            forced[role]=p.poll() is None
            if forced[role]:p.terminate();p.wait(timeout=10)
        for handle in handles:handle.close()
        for out in outputs:cleanup(out)
    result=dict(case=args.case,level=level,policy=policy,join_delay=delay,peers={})
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
        if (out/'lighting-policy.csv').exists():
            with (out/'lighting-policy.csv').open() as f:policy_rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(f)]
        stable=[r for r in policy_rows if r['tick']>600]
        expected_policy={'stock':0,'glow_overshield':4,'players':-1}[policy]
        expected_shield=int(settings[role]['shield_glow']) if policy=='players' else int(policy!='stock')
        correct=[r for r in stable if r['policy']==expected_policy and r['shield_enabled']==expected_shield]
        failures=errors+re.findall(r'(?im)^.*(?:crash: exception|exception halt|assertion failed|shader.*(?:error|failed)|GL_INVALID).*$',log)
        debug=(out/'debug.txt').read_text(errors='replace') if (out/'debug.txt').exists() else ''
        failures += re.findall(r'(?im)^.*(?:i expected a function name|not while playing in another).*$',debug)
        config_paths=re.findall(r'^halo-linux: settings: ([^\r\n]*config\.toml)\s*$',log,re.M)
        config_ok=bool(config_paths) and all(Path(v.strip()).resolve()==(out/'config.toml').resolve() for v in config_paths)
        result['peers'][role]=dict(exit=p.returncode,forced_stop=forced[role],two_player_samples=len(both),
            one_player_samples=sum(len(r['players'])==1 for r in rows),
            first_two_player_tick=both[0]['tick'] if both else None,respawns=respawns,inventories=inventories,
            shoot_events=log.count('shoots player'),kill_events=log.count('first player kills'),
            pickup_events=log.count('stands on weapon'),policy_samples=len(stable),correct_policy_samples=len(correct),
            policy_values=sorted(set(r['policy'] for r in stable)),config_verified=config_ok,failures=failures)
    result['agreement']=judge_run(base/'host/stderr.log',base/'client/stderr.log',SimpleNamespace(seconds=45,skip=8),TOLERANCES)
    result['paired_snapshot_seconds']=result['agreement'].get('figures',{}).get('seconds',0)
    result['passed']=report_passed(result)
    (base/'report.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2),flush=True)
    return 0 if result['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
