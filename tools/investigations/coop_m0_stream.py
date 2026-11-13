"""Exercise real stream buffering/KCP over private Windows loopback UDP.

The relay injects seeded faults independently of application backpressure.
Virtual milliseconds measure scheduled progress, not real-world latency/cost.
No game processes, production edits, external peers or retail assets are used.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from harness import constant, enum_with, function, mutated, structure


def main():
    output=ROOT/'build/m0/stream'
    output.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'port/linux/src/p2p.c').read_text()
    constants=('MAXIMUM_STREAMS','MAXIMUM_INNER_SIZE','STREAM_BUFFER_SIZE','STREAM_CHUNK_SIZE',
               'STREAM_WINDOW','KCP_MTU','LOOP_INTERVAL','STREAM_LINGER_TIME')
    types='\n'.join(f'#define {name} {constant(source,name)}' for name in constants)
    types+='\n'+'\n'.join(enum_with(source,anchor) for anchor in ('_packet_ping =','_stream_open =','_stream_awaiting_open,'))
    types+='\n'+structure(source,'stream')
    names=('kcp_output','stream_new','stream_free','stream_message','stream_local_closed',
           'stream_flush_pending','stream_update','stream_readable')
    functions='\n'.join(function(source,name) for name in names)
    (output/'stream_types.inc').write_text(types)
    (output/'stream_functions.inc').write_text(functions)
    binary=output/'stream.exe'
    command=['clang','-m32','-std=gnu89','-O2','-Wall','-Werror','-Wno-unused-function',
        '-D_CRT_SECURE_NO_WARNINGS','-I',str(output),'-I',str(ROOT/'port/third_party/kcp'),
        str(ROOT/'tools/investigations/coop_m0_stream.c'),str(ROOT/'port/third_party/kcp/ikcp.c'),
        '-o',str(binary),'-lws2_32']
    compiled=subprocess.run(command,capture_output=True,text=True)
    (output/'build.log').write_text(compiled.stdout+compiled.stderr)
    compiled.check_returncode()
    results={'revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'source_sha256':hashlib.sha256(source.encode()).hexdigest(),
        'inputs_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in (
            'tools/investigations/coop_m0_stream.py','tools/investigations/coop_m0_stream.c',
            'port/third_party/kcp/ikcp.c','port/third_party/kcp/ikcp.h')},
        'extracted_functions':list(names),'cases':[],
        'limits':'Virtual clock, synthetic payload, pre-opened streams; no crypto/admission, game TCP, WebRTC or full co-op qualification.'}
    for case in ('normal','impaired','wire-stall','sink-stall','combined'):
        for seed in (1,42,8675309):
            run=subprocess.run([str(binary),case,str(seed)],capture_output=True,text=True,timeout=60)
            entry={'case':case,'seed':seed,'exit_code':run.returncode,'stderr':run.stderr}
            try:entry['observations']=json.loads(run.stdout)
            except json.JSONDecodeError:entry['stdout']=run.stdout
            results['cases'].append(entry)
    # Deliberate faults stay in separate generated copies and must fail the
    # intended byte-integrity or queue-bound oracle, not merely exit nonzero.
    results['negative_controls']=[]
    for name,before,after,reason in (
        ('payload', 'memcpy(message + 1, data, (size_t)size);',
         'memcpy(message + 1, data, (size_t)size); if(size)message[1]^=1;', 'exact ordered payload'),
        ('backpressure', 'ikcp_waitsnd(stream->kcp) < STREAM_WINDOW',
         '1', 'production backpressure bounds')):
        folder=output/name
        folder.mkdir(exist_ok=True)
        (folder/'stream_types.inc').write_text(types)
        (folder/'stream_functions.inc').write_text(mutated(functions,before,after))
        control_command=[str(folder) if arg==str(output) else str(folder/'stream.exe') if arg==str(binary) else arg for arg in command]
        built=subprocess.run(control_command,capture_output=True,text=True)
        (folder/'build.log').write_text(built.stdout+built.stderr)
        built.check_returncode()
        run=subprocess.run([str(folder/'stream.exe'),'combined','1'],capture_output=True,text=True,timeout=60)
        results['negative_controls'].append({'name':name,'exit_code':run.returncode,
            'expected_reason':reason,'detected':run.returncode==1 and reason in run.stderr,'stderr':run.stderr})
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    return int(any(case['exit_code'] for case in results['cases']) or
               not all(case['detected'] for case in results['negative_controls']))


if __name__=='__main__':
    sys.exit(main())
