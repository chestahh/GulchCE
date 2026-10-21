"""Exercise the diagnostic writer, sampling window and unchanged upload actions."""
from pathlib import Path
import os
import shutil
import subprocess

import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]


def build_run(tmp_path, code, sources=(), headers=None):
    compiler = shutil.which('clang')
    assert compiler, 'clang is required'
    for name, contents in (headers or {}).items():
        (tmp_path / name).write_text(contents)
    source = tmp_path / 'test.c'
    source.write_text(code)
    target = ['--target=i686-pc-windows-msvc', '-fuse-ld=lld'] if os.name == 'nt' else []
    binary = tmp_path / ('test.exe' if os.name == 'nt' else 'test')
    args = [compiler, *target, '-D_CRT_SECURE_NO_WARNINGS', '-std=gnu89', '-Wall', '-Wextra',
            '-Werror', '-I'+str(tmp_path), str(source), *map(str, sources), '-o', str(binary)]
    if os.name != 'nt': args.append('-pthread')
    result = subprocess.run(args, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(binary)], cwd=tmp_path, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr


def test_capture_rotation_limit_and_unwritable_path(tmp_path):
    headers = {}
    if os.name == 'nt':
        headers['pthread.h'] = '''typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static int pthread_mutex_lock(pthread_mutex_t *p) { (void)p; return 0; }
static int pthread_mutex_unlock(pthread_mutex_t *p) { (void)p; return 0; }
'''
    build_run(tmp_path, r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
void host_geometry_begin(const char *);
void host_geometry_log(const char *);
int main(void) {
 FILE *f; char text[1024], line[1024]; long bytes; int i, found=0;
 host_geometry_begin("."); host_geometry_log("test marker");
 host_geometry_begin(".");
 f=fopen("android-geometry-test.log.previous","rb"); assert(f);
 while(fgets(text,sizeof(text),f)) if(strstr(text,"test marker")) found=1;
 fclose(f); assert(found);
 memset(line,'x',sizeof(line)-1); line[sizeof(line)-1]=0;
 for(i=0;i<10000;i++) host_geometry_log(line);
 f=fopen("android-geometry-test.log","rb"); assert(f);
 fseek(f,0,SEEK_END); bytes=ftell(f); assert(bytes<=8*1024*1024 && bytes>7*1024*1024);
 fseek(f,-64,SEEK_END); memset(text,0,sizeof(text)); fread(text,1,64,f); fclose(f);
 assert(strstr(text,"log limit reached"));
 host_geometry_begin("missing/directory"); host_geometry_log("must not crash");
 return 0;
}
''', [ROOT/'port/android/host/geometry_diagnostics.c'], headers)


def test_sampling_and_bounded_error_drain(tmp_path):
    source = (ROOT/'port/linux/src/android_geometry_diagnostics.c').read_text()
    (tmp_path/'android_geometry_diagnostics.h').write_text(
        (ROOT/'port/linux/src/android_geometry_diagnostics.h').read_text())
    # Compile the entire production implementation against a deterministic GL stub.
    code = r'''
#define HALO_ANDROID 1
#define HALO_GLES 1
#include <assert.h>
#include <stdarg.h>
static unsigned int reads, pending, logs;
typedef unsigned int GLenum;
#define GL_NO_ERROR 0
#define GL_VENDOR 1
#define GL_RENDERER 2
#define GL_VERSION 3
#define GL_SHADING_LANGUAGE_VERSION 4
static struct { int base_vertex, s3tc; } xgpu_capabilities={1,1};
static unsigned int glGetError(void) { reads++; if(pending) {pending--;return 0x502;} return 0; }
static const unsigned char *glGetString(unsigned int n) { (void)n; return (const unsigned char *)"test"; }
static void platform_log(const char *f,...) { (void)f; logs++; }
'''+source+r'''
int main(void) {
 unsigned int before; unsigned long i;
 geometry_test_present(61); assert(!geometry_test_sample);
 before=reads; pending=1; geometry_test_error("quiet",0); assert(reads==before);
 geometry_test_present(62); assert(pending==0 && geometry_test_sample);
 pending=20; before=reads; geometry_test_error("bounded",1); assert(reads-before==8 && pending==12);
 pending=0; geometry_test_present(500); assert(!geometry_test_sample);
 geometry_test_map("custom_maps/test"); assert(geometry_test_sample);
 geometry_test_present(501); assert(geometry_test_sample);
 geometry_test_present(800); assert(!geometry_test_sample);
 before=logs; for(i=0;i<25000;i++) geometry_test_log("limit"); assert(logs-before<=20000);
 return 0;
}
'''
    build_run(tmp_path, code, headers={'xgpu.h':''})


@pytest.mark.parametrize('platform',['desktop','web'])
def test_other_platforms_have_no_diagnostic_calls(tmp_path, platform):
    header=(ROOT/'port/linux/src/android_geometry_diagnostics.h').read_text()
    code=('#define HALO_WEB 1\n#define HALO_GLES 1\n' if platform=='web' else '')+header+'''
#ifdef HALO_GEOMETRY_DIAGNOSTICS
#error diagnostics enabled on an unaffected platform
#endif
int main(void) { return 0; }
'''
    build_run(tmp_path,code)


def test_host_upload_and_wait_keep_original_actions(tmp_path):
    host=(ROOT/'port/android/host/host_gl.c').read_text()
    code=r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
typedef unsigned int GLenum;
typedef void *GLsync;
#define GL_MAP_WRITE_BIT 2
#define GL_MAP_UNSYNCHRONIZED_BIT 32
#define GL_SYNC_FLUSH_COMMANDS_BIT 1
#define GL_ALREADY_SIGNALED 3
#define GL_CONDITION_SATISFIED 4
#define FRAME_FENCE_SLOTS 8
static GLsync frame_fences[8];
static unsigned int events, subdata, unmaps, waits, deletes, wait_status, fail_map, fail_unmap;
static unsigned char output[16];
static void host_geometry_event(const char *f,...) { (void)f;events++; }
static void *glMapBufferRange(uint32_t t,uint32_t o,uint32_t s,unsigned int flags) {
 (void)t;(void)o;(void)s;assert(flags==(2|32));return fail_map?0:output;
}
static void glBufferSubData(uint32_t t,uint32_t o,uint32_t s,const void *p) {
 (void)t;(void)o;subdata++;memcpy(output,p,s);
}
static int glUnmapBuffer(uint32_t t) { (void)t;unmaps++;return !fail_unmap; }
static GLenum glClientWaitSync(GLsync s,unsigned int f,unsigned long long ns) {
 assert(s && f==1 && ns==1000000000ull);waits++;return wait_status;
}
static void glDeleteSync(GLsync s) { assert(s);deletes++; }
'''+function(host,'host_gl_buffer_write')+function(host,'host_gl_wait_frame')+r'''
int main(void) {
 const char input[]="payload";
 host_gl_buffer_write(1,0,sizeof(input),input); assert(!memcmp(input,output,sizeof(input)));
 assert(unmaps==1 && subdata==0 && events==0);
 fail_map=1; host_gl_buffer_write(1,0,sizeof(input),input); assert(subdata==1 && events==1);
 fail_map=0; fail_unmap=1; host_gl_buffer_write(1,0,sizeof(input),input); assert(events==2 && subdata==1);
 frame_fences[0]=(void *)1;wait_status=GL_ALREADY_SIGNALED;host_gl_wait_frame(0);
 assert(waits==1 && deletes==1 && !frame_fences[0] && events==2);
 frame_fences[0]=(void *)1;wait_status=0x911b;host_gl_wait_frame(0);
 assert(waits==2 && deletes==2 && !frame_fences[0] && events==3);
 return 0;
}
'''
    build_run(tmp_path,code)
