"""Production cache functions, failure paths and deterministic I/O counts."""
from pathlib import Path
import subprocess

from harness import function, structure
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


def test_ce_scan_growth_refresh_and_allocation_failure(tmp_path):
    source = (ROOT / "port/linux/game/custom_edition_maps.c").read_text()
    cache = source[source.index("#define IDENTITY_CACHE_SIZE"):source.index("static void custom_edition_maps_forget(")]
    code = "\n".join(structure(source, name) for name in ("custom_edition_map", "custom_edition_maps_globals"))
    code += '\nstatic struct custom_edition_maps_globals custom_edition_maps_globals;\n'
    code += 'static wchar_t const default_description[] = L"Halo Custom\\r\\nEdition map";\n'
    code += '#define DESCRIPTION_OWN_NAME 1\n#define DESCRIPTION_BASE_NAME 2\n' + cache
    code += "\n".join(function(source, name) for name in (
        "custom_edition_maps_forget", "custom_edition_map_description_read", "custom_edition_map_add",
        "custom_edition_map_compare", "custom_edition_maps_scan_files", "custom_edition_maps_tell_versions_apart",
        "custom_edition_maps_look_for"))
    (tmp_path / "under_test.inc").write_text(code)
    execute(tmp_path, (ROOT / "tools/harness/tests/ce_scan_cache.c").read_text())


def test_saved_index_writes_rescan_and_failure(tmp_path):
    source = (ROOT / "source/saved games/saved_game_files.c").read_text()
    end = source.index("} mapfile_copy;") + len("} mapfile_copy;")
    start = source.rindex("static struct", 0, end)
    code = structure(source, "enumerated_saved_game_file") + "\n" + source[start:end] + "\n"
    code += "\n".join(function(source, name) for name in (
        "mapfile_copy_forget", "mapfile_copy_read", "get_nth_entry_in_mapfile", "set_nth_entry_in_mapfile",
        "append_entry_to_mapfile", "remove_nth_entry_in_mapfile", "saved_game_files_notify_memory_units_changed",
        "enumerate_saved_game_files_start", "enumerate_saved_game_file", "enumerate_saved_game_files_end",
        "saved_game_files_dispose"))
    (tmp_path / "under_test.inc").write_text(code)
    execute(tmp_path, (ROOT / "tools/harness/tests/saved_index_cache.c").read_text())


def test_save_drive_success_failure_root_and_invalidation(tmp_path):
    source = (ROOT / "port/linux/src/xbox_files.c").read_text()
    (tmp_path / "posix.h").write_text((ROOT / "port/linux/src/posix.h").read_text())
    execute(tmp_path, r'''
#include "posix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static void pthread_mutex_lock(int *m){assert(!*m);*m=1;}
static void pthread_mutex_unlock(int *m){assert(*m);*m=0;}
static int stats,creates,exists,denied;
int posix_stat(char const *p,struct posix_file_information *i){(void)p;stats++;i->flags=_posix_file_is_directory;return exists ? 0 : -1;}
int posix_make_directory(char const *p){(void)p;creates++;if(denied)return -1;exists=1;return 0;}
''' + function(source, "save_drive_prepare") + r'''
int main(void){int i;
    for(i=0;i<300;i++)save_drive_prepare("saves/z",25);
    assert(stats==1 && creates==1);
    printf("Save drive 300 resolutions: %d directory stats (baseline: 300)\n",stats);
    save_drive_prepare("other/z",25);assert(stats==2);
    exists=0;denied=1;save_drive_prepare("missing/z",25);save_drive_prepare("missing/z",25);assert(creates==3);
    denied=0;save_drive_prepare("missing/z",25);assert(creates==4);
    exists=0;save_drive_prepare(NULL,0);save_drive_prepare("missing/z",25);assert(creates==5);
    return 0;
}
''')


def test_directory_creation_recovers_removed_drive(tmp_path):
    source = (ROOT / "port/linux/src/xbox_files.c").read_text()
    execute(tmp_path, r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
typedef int BOOL;
typedef char const *LPCSTR;
typedef void *LPSECURITY_ATTRIBUTES;
#define WINAPI
#define TRUE 1
#define FALSE 0
static int translations,clears,attempts,missing=1,denied,last_error;
static void platform_translate_path(char const *in,char *out,unsigned long size){
    translations++;snprintf(out,size,"%s",in);
}
static void save_drive_prepare(char const *path,unsigned drive){
    assert(!path && !drive);clears++;missing=0;
}
static int posix_make_directory(char const *path){
    (void)path;attempts++;if(denied){errno=EACCES;return -1;}
    if(missing){errno=ENOENT;return -1;}return 0;
}
static int rmdir(char const *path){(void)path;if(denied){errno=EACCES;return -1;}return 0;}
static void platform_set_last_error_from_errno(int e){last_error=e;}
''' + function(source, "CreateDirectoryA") + function(source, "RemoveDirectoryA") + r'''
int main(void){
    assert(CreateDirectoryA("z:/saved",NULL));
    assert(attempts==2 && translations==2 && clears==1);
    denied=1;assert(!CreateDirectoryA("z:/saved",NULL));
    assert(attempts==3 && clears==1 && last_error==EACCES);
    assert(!RemoveDirectoryA("z:/saved") && clears==1);
    denied=0;assert(RemoveDirectoryA("z:/saved") && clears==2);
    assert(CreateDirectoryA("z:/saved",NULL) && attempts==4);
    return 0;
}
''')
