"""Production cache functions, failure paths and deterministic I/O counts."""
from pathlib import Path
import subprocess

from harness import function
from test_mcc_identity import compile_tool

ROOT = Path(__file__).resolve().parents[1]


def execute(tmp_path, source):
    path = tmp_path / "case.c"
    path.write_text(source)
    exe = compile_tool(tmp_path, [path])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip())


def test_mcc_polling_and_fresh_snapshots(tmp_path):
    source = (ROOT / "port/linux/game/mcc_precache.c").read_text()
    state = source[source.index("#define SNAPSHOT_KEPT_MS"):source.index("static void mcc_snapshot_forget(void)\n{")]
    code = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mcc_precache.h"
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static void pthread_mutex_lock(int *m){assert(!*m);*m=1;}
static void pthread_mutex_unlock(int *m){assert(*m);*m=0;}
static unsigned long now;
static int reads,fail;
static uint32_t stamp=1;
static unsigned long system_milliseconds(void){return now;}
static int mcc_precache_files(char const *name,struct mcc_precache_stamp *files){
    unsigned i;reads+=9;
    if(fail || !name)return 0;
    memset(files,0,sizeof(*files)*9);
    for(i=0;i<9;i++)files[i].written_low=stamp;
    return 1;
}
''' + state + "\n" + "\n".join(function(source, name) for name in (
        "mcc_snapshot_forget", "mcc_precache_snapshot", "mcc_precache_snapshot_poll")) + r'''
int main(void){
    struct mcc_precache_snapshot before,after;int i;
    for(i=0;i<300;i++){now=(unsigned long)i*1000/60;assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&before));}
    printf("MCC 300 polls over five seconds: %d file-stat requests (fresh baseline: 2700)\n",reads);
    assert(reads==90);
    now=5000;assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&before));
    stamp++;now+=100;
    assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&after));
    assert(!memcmp(&before,&after,sizeof(before)));
    assert(mcc_precache_snapshot("mcc_maps/p/a",&after));
    assert(memcmp(&before,&after,sizeof(before)));
    assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&before));assert(!memcmp(&before,&after,sizeof(before)));
    fail=1;assert(!mcc_precache_snapshot("mcc_maps/p/a",&after));
    assert(!mcc_precache_snapshot_poll("mcc_maps/p/a",&after));
    fail=0;assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&after));
    stamp++;mcc_snapshot_forget();assert(mcc_precache_snapshot_poll("mcc_maps/p/a",&after));
    assert(after.files[0].written_low==stamp);
    stamp++;assert(mcc_precache_snapshot_poll("mcc_maps/q/a",&after));assert(after.files[0].written_low==stamp);
    now=~0ul-100;mcc_snapshot_forget();assert(mcc_precache_snapshot_poll("mcc_maps/q/a",&before));
    now=450;stamp++;assert(mcc_precache_snapshot_poll("mcc_maps/q/a",&after));assert(after.files[0].written_low==stamp);
    return 0;
}
'''
    execute(tmp_path, code)


def test_mcc_exact_path_before_case_fallback(tmp_path):
    source = (ROOT / "port/linux/src/mcc_local_package.c").read_text()
    (tmp_path / "posix.h").write_text((ROOT / "port/linux/src/posix.h").read_text())
    execute(tmp_path, r'''
#include "posix.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int stats,listings,exists=1;
int posix_stat(char const *p,struct posix_file_information *i){(void)p;(void)i;stats++;return exists ? 0 : -1;}
int posix_find_entry_case_insensitive(char const *p,char const *n,char *out,unsigned long size){
    (void)p;(void)n;listings++;snprintf(out,size,"MiXeD");return 1;
}
''' + function(source, "child") + r'''
int main(void){char path[1024];int i;
    for(i=0;i<300;i++)assert(child("root","exact",path));
    assert(stats==300 && listings==0 && !strcmp(path,"root/exact"));
    printf("MCC 300 exact path components: %d directory listings (baseline: 300)\n",listings);
    exists=0;assert(child("root","mixed",path));assert(listings==1 && !strcmp(path,"root/MiXeD"));
    return 0;
}
''')
