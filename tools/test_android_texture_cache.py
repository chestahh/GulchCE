"""Check normal cached uploads and the isolated Android state comparison."""
from pathlib import Path
import os
import shutil
import subprocess
import pytest
from harness import function, structure

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("platform", ["android", "desktop", "web"])
def test_refresh(tmp_path, platform):
    source = (ROOT / "port/linux/src/xbox_textures.c").read_text(encoding="utf-8")
    compiler = shutil.which("clang")
    if not compiler:
        pytest.skip("clang required")
    defines = "#define HALO_ANDROID 1\n" if platform == "android" else "#define HALO_WEB 1\n" if platform == "web" else ""
    code = defines + r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef unsigned int DWORD, GLuint, GLenum, D3DCOLOR;
typedef int BOOL;
typedef unsigned long long Uint64;
#define TRUE 1
#define FALSE 0
#define GL_TEXTURE_2D 1
#define GL_TEXTURE_CUBE_MAP 2
#define GL_TEXTURE_3D 3
#define D3DFORMAT_FORMAT_MASK 255
#define D3DFORMAT_FORMAT_SHIFT 0
#define PLATFORM_PHYSICAL_TO_VIRTUAL(x) ((void *)(uintptr_t)(x))
struct xgpu_texture_description {
 unsigned long width, height, depth, levels, format;
 BOOL cube_map;
};
static unsigned long serial=1, generation=1, uploads, next_texture;
static BOOL mcc;
static unsigned long memory_watch_serial(void) { return serial; }
static unsigned long memory_watch_generation(unsigned long a,unsigned long s) { return generation; }
static void memory_watch_protect(unsigned long a,unsigned long s) {}
static BOOL xgpu_mcc_texture_get(const DWORD *r,const D3DCOLOR *p,GLuint *t,GLenum *g,struct xgpu_texture_description *d) {
 if (!mcc) return FALSE; *t=999; return TRUE;
}
static BOOL config_boolean(const char *s) { return FALSE; }
static void platform_log(const char *s,...) {}
static void xgpu_texture_describe(DWORD f,DWORD s,struct xgpu_texture_description *d) {
 memset(d,0,sizeof(*d));d->width=d->height=16;d->depth=d->levels=1;
}
static BOOL texture_size_supported(const struct xgpu_texture_description *d) { return TRUE; }
static unsigned long xgpu_texture_face_size(const struct xgpu_texture_description *d) { return 1024; }
static unsigned long xgpu_texture_level_offset(const struct xgpu_texture_description *d,int l) { return 1024; }
static void glGenTextures(int n,GLuint *t) { *t=++next_texture; }
static void glDeleteTextures(int n,GLuint *t) {}
static void xgpu_gl_state_invalidate(void) {}
static long hud_hires_override_find(unsigned long a,unsigned long w,unsigned long h,unsigned long b) { return -1; }
static GLuint hud_hires_override_texture(long i,unsigned long *l) { return 0; }
static BOOL platform_is_contiguous(void *p) { return TRUE; }
static unsigned char custom_edition_texels_order(unsigned long a) { return 0; }
static void xgpu_statistics_texture_upload(unsigned long s) {}
static void upload(GLuint t,GLenum g,const struct xgpu_texture_description *d,const unsigned char *b,const D3DCOLOR *p,unsigned char c) { uploads++; }
'''
    code += source[source.index("struct texture_entry\n"):source.index("/* (every bit of the three mixed")]
    code += function(source, "bucket_index") + "\n" + function(source, "palette_hash")
    code += r'''
static GLuint texture_entry_result(struct texture_entry *e,GLenum *t,struct xgpu_texture_description *d) {
 *t=e->target;*d=e->description;return e->texture;
}
'''
    code += function(source, "xgpu_texture_get")
    code += r'''
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void) {
 DWORD resource[5]={0,0x12340000,0,0,0};GLenum target;struct xgpu_texture_description description;
 GLuint original;unsigned long before;struct texture_entry *entry;
 texture_frame=1;
 original=xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==1);
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==1);
 texture_frame++;
 CHECK(xgpu_texture_get(resource,NULL,&target,&description)==original);
 CHECK(uploads==1);
 before=uploads;
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before);
 /* A normal tracked write must still refresh within the same frame. */
 serial++;generation++;
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before+1);
 entry=texture_buckets[bucket_index(resource[1],0,0)];CHECK(entry);
 entry->override=7;before=uploads;texture_frame++;
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before);
 entry->override=-1;entry->hud_upscale=55;texture_frame++;
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before);
 entry->hud_upscale=0;
 mcc=TRUE;texture_frame++;
 CHECK(xgpu_texture_get(resource,NULL,&target,&description)==999 && uploads==before);
 mcc=FALSE;
 /* An unseen texture and a palette variant have independent refresh state. */
 resource[1]+=4096;resource[3]=11;
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before+1);
 xgpu_texture_get(resource,NULL,&target,&description);CHECK(uploads==before+1);
 texture_frame++;
 xgpu_texture_get(resource,NULL,&target,&description);
 CHECK(uploads==before+1);
 return 0;
}
'''
    path = tmp_path / "cache.c"
    path.write_text(code, encoding="utf-8")
    binary = tmp_path / ("cache.exe" if os.name == "nt" else "cache")
    target = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"] if os.name == "nt" else []
    result = subprocess.run([compiler, *target, "-std=gnu89", str(path), "-o", str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr

    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_diagnostic_report(tmp_path):
    compiler = shutil.which("clang")
    if not compiler:
        pytest.skip("clang required")
    description = structure((ROOT / "port/linux/src/xgpu.h").read_text(encoding="utf-8"), "xgpu_texture_description")
    code = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int DWORD, GLenum;
typedef int BOOL;
typedef unsigned long long Uint64;
#define TRUE 1
#define FALSE 0
#define GL_NO_ERROR 0
#define GL_VENDOR 1
#define GL_RENDERER 2
#define GL_VERSION 3
static Uint64 clock_ms;
static Uint64 SDL_GetTicks(void) { return clock_ms; }
static unsigned int pending_error, error_queries, invalidations;
static GLenum glGetError(void) { unsigned int e=pending_error;pending_error=0;error_queries++;return e; }
static void xgpu_gl_state_invalidate(void) { invalidations++; }
static const unsigned char *glGetString(GLenum e) { return (const unsigned char *)"test GPU"; }
static void platform_log(const char *s, ...) {}
static struct { int s3tc;const char *shading_language; } xgpu_capabilities={0,"310 es"};
''' + description + "\n" + (ROOT / "port/android/guest/texture_cache_test.h").read_text(encoding="utf-8")
    code += r'''
int main(void) {
 struct xgpu_texture_description d={0};Uint64 started;int i;
 android_texture_test_begin_frame(1);
 d.width=d.height=16;d.depth=d.levels=1;
 started=android_texture_test_upload_begin();clock_ms=5;
 android_texture_test_upload_end(&d,TRUE,0,started);
 pending_error=0x502;
 android_state_test_refresh(42,0x1234,FALSE);
 android_state_test_texture(0,0x12340000,12);
 android_state_test_stage("after textures");
 if (invalidations!=1 || android_texture_test_errors!=1) return 4;
 i=error_queries;
 android_state_test_refresh(43,0,FALSE);
 android_state_test_stage("not sampled");
 if (invalidations!=2 || error_queries!=i) return 5;
 clock_ms=10000;android_texture_test_begin_frame(2);
 if (android_texture_test_uploads!=1 || android_texture_test_decoded!=1 || android_texture_test_upload_ms!=5) return 1;
 for (i=0;i<100000;i++) android_texture_test_log("bounded test report padding");
 if (android_texture_test_log_bytes>ANDROID_TEXTURE_TEST_LOG_LIMIT+768) return 2;
 if (!android_texture_test_file) return 3;
 fclose(android_texture_test_file);
 return 0;
}
'''
    path = tmp_path / "diagnostic.c"
    path.write_text(code, encoding="utf-8")
    binary = tmp_path / ("diagnostic.exe" if os.name == "nt" else "diagnostic")
    target = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"] if os.name == "nt" else []
    result = subprocess.run([compiler, *target, "-std=gnu89", "-Werror=format", str(path), "-o", str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    report = tmp_path / "android-state-test.log"
    report.write_text("previous launch evidence")
    result = subprocess.run([str(binary)], env={**os.environ, "HALO_DATA_ROOT": str(tmp_path)}, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert report.with_suffix(".log.previous").read_text() == "previous launch evidence"
    text = report.read_text()
    assert "geometry mirror bypass retained" in text
    assert "vendor=test GPU" in text
    assert "mode=state-refresh-each-draw; normal texture cache" in text
    assert "before draw preparation (earlier GL work) GL error=0x502" in text
    assert "texture_stage=0 data=12340000 format=0000000c" in text
    assert "not sampled" not in text
    assert "uploads=1 state_refreshes=2 decoded_DXT=1 upload_CPU_ms=5" in text
    assert report.stat().st_size < 600 * 1024


def test_state_refresh_integration():
    source = (ROOT / "port/linux/src/d3d8_gl.c").read_text(encoding="utf-8")
    prepare = function(source, "prepare_draw")
    assert prepare.index("android_state_test_refresh(") < prepare.index("bind_textures(")
    assert prepare.index("bind_textures(") < prepare.index("apply_raster_state(")
    # The comparison must keep program/uniform caches and Android geometry bypass.
    assert "entry->uniforms_serial == draw_uniforms_serial" in prepare
    mirror = function(source, "mirror_range")
    assert "#ifdef HALO_ANDROID" in mirror and "return FALSE;" in mirror
    # Preprocessing without Android must remove all diagnostic wiring.
    import re
    for guarded in re.findall(r"#ifdef HALO_ANDROID\n(.*?)#endif", prepare, re.S):
        prepare = prepare.replace("#ifdef HALO_ANDROID\n" + guarded + "#endif", "")
    assert "android_state_test_" not in prepare
