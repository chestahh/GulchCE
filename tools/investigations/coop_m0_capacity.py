"""Scale the existing real-function checkpoint oracle; no game assets or edits.

Generated copies retain the existing assertions and fake-world boundaries.
This measures serialization/reconstruction, not network delivery or game cost.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from harness import constant, read, mutated
from test_coop_checkpoint_recovery import generated


def main():
    output = ROOT / 'build/m0/local/capacity'
    output.mkdir(parents=True, exist_ok=True)
    maximum = constant(read('port/linux/game/network_distributed.h'), 'MAXIMUM_TRACKED_OBJECTS')
    ready_interval = constant(read('port/linux/game/network_objects.c') + read('source/cseries/cseries.h'), 'CLIENT_READY_INTERVAL_TICKS')
    original = read('tools/harness/tests/coop_checkpoint_recovery.c')
    results = {'production_object_limit': maximum, 'fixture_sha256': hashlib.sha256(original.encode()).hexdigest(), 'cases': []}
    for capacity in (128, maximum - 1, maximum):
        folder = output / str(capacity)
        folder.mkdir(exist_ok=True)
        for name, contents in generated():
            (folder/name).write_text(contents)
        fixture = mutated(original, '#define MAXIMUM_TRACKED_OBJECTS 128', f'#define MAXIMUM_TRACKED_OBJECTS {capacity}')
        # At most one packet per object plus barriers; capture capacity only.
        fixture = fixture.replace('snapshot_packets[32]', 'snapshot_packets[MAXIMUM_TRACKED_OBJECTS + 4]')
        fixture = fixture.replace('snapshot_sizes[32]', 'snapshot_sizes[MAXIMUM_TRACKED_OBJECTS + 4]')
        fixture = mutated(fixture, 'snapshot_count<32', 'snapshot_count<MAXIMUM_TRACKED_OBJECTS + 4')
        fixture = mutated(fixture, '#include <math.h>', '''#include <math.h>
__declspec(dllimport) int __stdcall QueryPerformanceCounter(__int64 *);
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(__int64 *);
static int m0_stall_type=-1;''')
        fixture = mutated(fixture, 'CHECK(distributed_message_epoch_valid(NONE,header,snapshot_sizes[i]),"ordered snapshot admitted while gameplay is frozen");', '''if(header->type==m0_stall_type){
            int tick,requests=client_ready_sends;
            for(tick=0;tick<10*TICKS_PER_SECOND;tick++){
                now++;network_objects_client_tick();
                CHECK(!distributed_checkpoint_ready()&&!prediction_ticks,"withheld reliable phase keeps gameplay producers frozen");
            }
            CHECK(client_ready_sends>requests,"withheld phase still requests recovery");
            printf("stall type=%d ticks=%d recovery_requests=%d\\n",m0_stall_type,10*TICKS_PER_SECOND,client_ready_sends-requests);
            m0_stall_type=-1;
        }
        CHECK(distributed_message_epoch_valid(NONE,header,snapshot_sizes[i]),"ordered snapshot admitted while gameplay is frozen");''')
        fixture = mutated(fixture, 'network_objects_client_asked(1,TRUE);\n    CHECK(snapshot_count>2', '''{
        __int64 begin,end,frequency;unsigned long bytes=0;int packet;
        CHECK(QueryPerformanceFrequency(&frequency)&&QueryPerformanceCounter(&begin),"QPC start");
        network_objects_client_asked(1,TRUE);
        CHECK(QueryPerformanceCounter(&end),"QPC end");
        for(packet=0;packet<snapshot_count;packet++)bytes+=snapshot_sizes[packet];
        printf("snapshot capacity=%d packets=%d bytes=%lu build_ms=%.6f qpc_frequency=%lld\\n",
            world_capacity,snapshot_count,bytes,1000.0*(end-begin)/frequency,frequency);
    }
    CHECK(snapshot_count>2''')
        fixture = mutated(fixture, 'setup();change=description(index);', '''setup();
    if(!strcmp(name,"snapshot-stall-objects"))m0_stall_type=_distributed_message_object_changes;
    if(!strcmp(name,"snapshot-stall-inventories"))m0_stall_type=_distributed_message_inventories;
    if(!strcmp(name,"snapshot-stall-marker"))m0_stall_type=_distributed_message_objects_synchronized;
    /* Match the production new-game retry initialization for timed stalls.
       The original fixture's zero interval suffices for its untimed oracle. */
    if(m0_stall_type!=-1){objects_client_ready_time=NONE;objects_client_ready_interval=M0_READY_INTERVAL;}
    if(!strcmp(name,"snapshot-vacant")){
        struct distributed_message_header *first,*last;
        client=0;distributed_checkpoint_epoch++;objects_host_machines[1].time=NONE;
        network_objects_client_asked(1,TRUE);
        first=(struct distributed_message_header *)snapshot_packets[0].bytes;
        last=(struct distributed_message_header *)snapshot_packets[snapshot_count-1].bytes;
        CHECK(snapshot_count==2&&first->type==_distributed_message_checkpoint&&
            last->type==_distributed_message_objects_synchronized,"vacant world barrier and marker only");
        CHECK(first->count==0&&last->count==0,"vacant world has no entries");
        client=1;distributed_checkpoint_epoch--;
        CHECK(!distributed_message_epoch_valid(NONE,first,snapshot_sizes[0])&&!distributed_checkpoint_ready(),"barrier freezes vacant client");
        CHECK(distributed_message_epoch_valid(NONE,last,snapshot_sizes[1])&&distributed_checkpoint_ready(),"marker releases vacant client");
        network_objects_handle_synchronized();
        printf("vacant packets=%d bytes=%u\\n",snapshot_count,snapshot_sizes[0]+snapshot_sizes[1]);
        return 0;
    }
    change=description(index);''')
        fixture = f'#define M0_READY_INTERVAL {ready_interval}\n' + fixture
        source = folder/'capacity.c'
        source.write_text(fixture)
        executable = folder/'capacity.exe'
        command = ['clang','-m32','-std=gnu99','-O2','-Wall','-Werror','-Wno-unused-function','-Wno-unused-variable',
                   '-D_CRT_SECURE_NO_WARNINGS=', '-I',str(ROOT/'tools/harness/include'),'-I',str(folder),str(source),'-o',str(executable),'-lkernel32']
        compiled = subprocess.run(command, capture_output=True, text=True)
        if compiled.returncode:
            raise RuntimeError(compiled.stdout + compiled.stderr)
        for case in ('snapshot-vacant', 'snapshot-empty', 'snapshot-batches',
                     'snapshot-stall-objects', 'snapshot-stall-inventories', 'snapshot-stall-marker'):
            for repeat in range(3):
                run = subprocess.run([str(executable),case], capture_output=True, text=True, timeout=60)
                results['cases'].append({'capacity':capacity,'case':case,'repeat':repeat,'exit_code':run.returncode,'output':run.stdout+run.stderr})
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    return int(any(case['exit_code'] for case in results['cases']))


if __name__ == '__main__':
    sys.exit(main())
