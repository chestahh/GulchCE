"""Exercise terrain selection, bounded fingerprints and uniform readback checks."""
from pathlib import Path
import os
import shutil
import subprocess
import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]


def run_c(tmp_path, code):
    compiler = shutil.which("clang")
    if not compiler:
        pytest.skip("clang required")
    path = tmp_path / "terrain.c"
    path.write_text(code)
    exe = tmp_path / ("terrain.exe" if os.name == "nt" else "terrain")
    target = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"] if os.name == "nt" else ["-m32"]
    result = subprocess.run([compiler, *target, "-std=gnu89", "-Werror=format", str(path), "-o", str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_material_scope_and_map_families(tmp_path):
    source = (ROOT / "port/linux/game/android_terrain_test.c").read_text()
    code = r'''
#include <string.h>
#include <stddef.h>
#define NONE -1
static int ce, pass, lookups;
static const char *name;
static int custom_edition_cache_tags_loaded(void) { return ce; }
static char *tag_get_name(long index) { lookups++;return (char *)name; }
static void android_terrain_test_scope(int value) { pass=value; }
'''
    code += function(source, "android_terrain_test_begin") + function(source, "android_terrain_test_end")
    code += r'''
int main(void) {
 name="levels\\extinction\\bitmaps\\extinction_terrain2";
 android_terrain_test_begin(3,1);if(pass || lookups) return 1;
 ce=1;android_terrain_test_begin(-1,1);if(pass || lookups) return 2;
 android_terrain_test_begin(3,1);if(pass!=1) return 3;
 android_terrain_test_end();if(pass) return 4;
 android_terrain_test_begin(3,2);if(pass!=2) return 5;
 name="levels\\extinction\\bitmaps\\cliff";
 android_terrain_test_begin(3,1);if(pass) return 6;
 name=NULL;android_terrain_test_begin(3,1);if(pass) return 7;
 ce=0;android_terrain_test_begin(3,2);if(pass) return 8;
 return 0;
}
'''
    run_c(tmp_path, code)


def test_uniform_readback_mismatch_and_missing_location(tmp_path):
    source = (ROOT / "port/android/guest/terrain_uniform_test.h").read_text()
    code = r'''
#include <string.h>
#include <stdarg.h>
typedef unsigned int GLuint;
typedef int GLint,BOOL;
#define FALSE 0
#define TRUE 1
static float values[4];
static int reads, reports;
static GLint glGetUniformLocation(GLuint p,const char *n) { return strcmp(n,"missing") ? 0 : -1; }
static void glGetUniformfv(GLuint p,GLint l,float *v) { reads++;memcpy(v,values,sizeof(values)); }
static void android_texture_test_log(const char *s,...) { reports++; }
'''
    code += function(source, "android_terrain_uniform_check")
    code += r'''
int main(void) {
 float expected[4]={1,2,3,4};unsigned long checked=0,mismatched=0;int i;
 memcpy(values,expected,sizeof(values));
 android_terrain_uniform_check(1,"c[12]",expected,&checked,&mismatched);
 if(checked!=1 || mismatched || reads!=1) return 1;
 android_terrain_uniform_check(1,"missing",expected,&checked,&mismatched);
 if(checked!=1 || reads!=1) return 2;
 values[1]=8;
 for(i=0;i<10;i++) android_terrain_uniform_check(1,"ps_c0[0]",expected,&checked,&mismatched);
 if(checked!=11 || mismatched!=10 || reports!=4) return 3;
 return 0;
}
'''
    run_c(tmp_path, code)


def test_source_fingerprint_bound(tmp_path):
    source = (ROOT / "port/android/guest/texture_cache_test.h").read_text()
    code = "#include <stdlib.h>\n#include <string.h>\n" + function(source, "android_terrain_test_hash")
    code += r'''
int main(void) {
 unsigned long cap=4UL*1024UL*1024UL,a,b;unsigned char *bytes=calloc(cap+1,1);
 if(!bytes) return 1;
 a=android_terrain_test_hash(bytes,cap+1);
 bytes[cap]=1;b=android_terrain_test_hash(bytes,cap+1);if(a!=b) return 2;
 bytes[cap-1]=1;b=android_terrain_test_hash(bytes,cap+1);if(a==b) return 3;
 free(bytes);return 0;
}
'''
    run_c(tmp_path, code)


def test_pass_hooks_and_uniform_capture_both_exit_paths():
    source = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_environment.c").read_text()
    for name in ("_rasterizer_environment_lightmap_draw", "_rasterizer_environment_diffuse_texture_draw"):
        body = function(source, name)
        assert body.count("return;") == 1
        assert body.index("android_terrain_test_begin") < body.index("rasterizer_set_vertex_shader_permutation")
        assert body.index("android_terrain_test_end") < body.index("return;")
    renderer = (ROOT / "port/linux/src/d3d8_gl.c").read_text()
    prepare = function(renderer, "prepare_draw")
    assert prepare.count("android_terrain_uniform_capture(entry)") == 2
    assert "glUniform" not in (ROOT / "port/android/guest/terrain_uniform_test.h").read_text()
