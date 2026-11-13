"""Observe readiness after failed reconstruction; no production changes.

These are characterization probes, not assertions that incomplete state should
be accepted. They extend the existing real-function recovery fixture and retain
its fake-world boundaries. The JSON records expected and observed behavior.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from harness import constant, mutated, read
from test_coop_checkpoint_recovery import generated

PROBE = r'''
    setup();change=description(index);
    if(!strncmp(name,"m0-",3)){
        int before;
        begin_checkpoint();
        objects_client_ready_time=now-1;
        objects_client_ready_interval=M0_READY_INTERVAL;
        if(!strcmp(name,"m0-allocation")){
            fail_allocation=TRUE;
            network_objects_handle_changes(&change,1);
            CHECK(!network_objects_client_has(index)&&objects_client_ask_again,"failed construction requests repair");
        }else{
            network_objects_handle_changes(&change,1);
            CHECK(network_objects_client_has(index),"positive object construction control");
            if(!strcmp(name,"m0-inventory")){
                weapon=DATUM_INDEX_NEW(4,0x1234);
                inventory=inventory_for(index,weapon);
                distributed_client_apply_inventory(&inventory);
                CHECK(objects_client_retried_inventory_count==1,"missing weapon defers inventory");
            }
        }
        CHECK(!distributed_checkpoint_ready(),"barrier blocks gameplay before marker");
        finish_snapshot();network_objects_handle_synchronized();
        before=prediction_ticks;network_objects_client_tick();
        printf("ready=%d objects_synchronized=%d object_present=%d pending_inventories=%d producers=%d\n",
            distributed_checkpoint_ready(),objects_client_synchronized,network_objects_client_has(index),
            objects_client_retried_inventory_count,prediction_ticks-before);
        if(!strcmp(name,"m0-allocation")){
            CHECK(!objects_client_synchronized&&!network_objects_client_has(index),"failed object remains absent at marker");
            fail_allocation=FALSE;now++;
            network_objects_handle_changes(&change,1);
            CHECK(network_objects_client_has(index),"later successful reconstruction repairs object");
        }else if(!strcmp(name,"m0-inventory")){
            CHECK(objects_client_retried_inventory_count==1,"marker does not resolve missing weapon");
            add_replica(4,_object_type_weapon);network_objects_client_tick();
            CHECK(!objects_client_retried_inventory_count&&object_get(index)->unit.weapon_object_indices[0]==weapon,
                "later dependency resolves queued inventory");
        }
        return 0;
    }
'''


def main():
    output = ROOT / 'build/m0/readiness'
    output.mkdir(parents=True, exist_ok=True)
    source = read('tools/harness/tests/coop_checkpoint_recovery.c')
    source = mutated(source, 'setup();change=description(index);', PROBE)
    interval = constant(read('port/linux/game/network_objects.c') + read('source/cseries/cseries.h'),
                        'CLIENT_READY_INTERVAL_TICKS')
    source = f'#define M0_READY_INTERVAL {interval}\n' + source
    sources = dict(generated())
    sources['ready.c'] = source
    for name, contents in sources.items():
        (output / name).write_text(contents)
    binary = output / 'ready.exe'
    compiled = subprocess.run(['clang', '-m32', '-std=gnu99', '-O2', '-Wall', '-Werror',
        '-Wno-unused-function', '-Wno-unused-variable', '-D_CRT_SECURE_NO_WARNINGS=',
        '-I', str(ROOT/'tools/harness/include'), '-I', str(output), str(output/'ready.c'),
        '-o', str(binary)], capture_output=True, text=True)
    (output/'build.log').write_text(compiled.stdout + compiled.stderr)
    compiled.check_returncode()
    results = {'revision': subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'generated_sha256': {name: hashlib.sha256(text.encode()).hexdigest() for name,text in sources.items()},
        'desired_behavior': 'A readiness claim must distinguish ordered receipt from successful persistent-state reconstruction.',
        'cases': []}
    for case in ('m0-complete', 'm0-allocation', 'm0-inventory'):
        run = subprocess.run([str(binary), case], capture_output=True, text=True, timeout=30)
        results['cases'].append({'case':case,'exit_code':run.returncode,'output':run.stdout+run.stderr})
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    return int(any(case['exit_code'] for case in results['cases']))


if __name__ == '__main__':
    sys.exit(main())
