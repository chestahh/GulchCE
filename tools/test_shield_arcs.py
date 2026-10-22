"""Exercise the real depleted-shield gates and private sprite asset lifecycle."""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

import pytest
from harness import function, structure
from extract_shield_arcs import extract

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def arcs_tool(tmp_path_factory):
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("clang required")
    work = tmp_path_factory.mktemp("shield-arcs")
    source = (ROOT / "port/linux/game/shield_arcs.inl").read_text()
    source = source[:source.index("void shield_arcs_draw(")]
    source = re.sub(r'^#include.*$', '', source, flags=re.M)
    bitmap = (ROOT / "source/bitmaps/bitmap_group.h").read_text()
    types = '\n'.join(structure(bitmap, n) for n in ["bitmap_data", "bitmap_group_sprite", "bitmap_group_sequence", "bitmap_group"])
    preamble = r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>
typedef int boolean;
typedef unsigned char byte;
typedef unsigned short word;
typedef float real;
typedef union { float n[4]; struct {float x0,x1,y0,y1;}; } real_rectangle2d;
typedef struct {float x,y;} real_point2d;
union point2d {short n[2];};
struct tag_block {long count;void *address,*definition;};
struct tag_data {long size;char pad[16];};
#define NONE (-1)
#define TRUE 1
#define FALSE 0
#define TEST_FLAG(value,bit) ((value)&(1U<<(bit)))
enum {_object_type_biped=0,_object_dead_bit=0,_object_shield_depleted_bit=1};
struct object_datum {struct {short type;unsigned damage_flags;float shield_vitality;short shield_stun_ticks;} object;};
static int enhanced=1,arms_enabled=1,creates,deletes,fail_texture;
static int shield_look_enabled(void) {return enhanced;}
int config_boolean(const char *name) {(void)name;return arms_enabled;}
void platform_log(const char *format,...) {(void)format;}
static const char *asset_path;
static FILE *asset_open(const char *path,const char *mode) {(void)path;return fopen(asset_path,mode);}
#define fopen asset_open
static void checked_free(void *p) {assert(p);free(p);}
#define free checked_free
'''
    stubs = r'''
static int rasterizer_bitmap_new(struct bitmap_data *b) {
 assert(b->flags==3);assert(b->mipmap_count==2);assert(b->pixels_size==172032);
 if(fail_texture && creates==1)return 0;
 b->hardware_format=(void*)1;creates++;return 1;
}
static void rasterizer_bitmap_changed(struct bitmap_data *b) {assert(b->base_address);}
static void rasterizer_bitmap_delete(struct bitmap_data *b) {assert(b->hardware_format);b->hardware_format=NULL;deletes++;}
static void *table;
static long table_count;
void *cache_files_tag_instances(long *count) {*count=table_count;return table;}
void cache_files_set_tag_instances(void *p,long count) {table=p;table_count=count;}
'''
    main = r'''
int main(int argc,char **argv) {
 struct object_datum object={{0,2,0.f,30}};
 struct shield_arcs_tag original[10];
 int i,expected;
 assert(argc==3);asset_path=argv[2];
 assert(shield_arcs_active(&object,0));assert(shield_arcs_active(&object,1));
 enhanced=0;assert(!shield_arcs_active(&object,0));enhanced=1;
 arms_enabled=0;assert(!shield_arcs_active(&object,1));assert(shield_arcs_active(&object,0));arms_enabled=1;
 assert(!shield_arcs_active(NULL,0));
 object.object.damage_flags=3;assert(!shield_arcs_active(&object,0));
 object.object.damage_flags=0;assert(!shield_arcs_active(&object,0));object.object.damage_flags=2;
 object.object.shield_vitality=.01f;assert(!shield_arcs_active(&object,0));object.object.shield_vitality=0;
 object.object.shield_stun_ticks=0;assert(!shield_arcs_active(&object,0));object.object.shield_stun_ticks=30;
 object.object.type=1;assert(!shield_arcs_active(&object,0));
 assert(shield_arcs_random(123)==shield_arcs_random(123));assert(shield_arcs_random(123)!=shield_arcs_random(124));
 table=original;table_count=10;expected=!strcmp(argv[1],"valid");fail_texture=!strcmp(argv[1],"texture-failure");
 shield_arcs_unload(); /* safe before any lazy load */
 for(i=0;i<3;i++) {
  creates=deletes=0;
  assert(shield_arcs_load()==expected);
  assert(shield_arcs_load()==expected); /* only one attempt per map */
  assert(table_count==(expected?11:10));
  if(expected) {assert(creates==2);assert(table!=original);assert(shield_arcs_table[10].address==&shield_arcs_bitmap_group);}
  else assert(table==original);
  shield_arcs_unload();assert(table==original);assert(table_count==10);assert(deletes==creates);
  assert(!shield_arcs_asset && !shield_arcs_table && shield_arcs_index==NONE);
  shield_arcs_unload();
 }
 return 0;
}
'''
    (work / "test.c").write_text(preamble + types + stubs + source + main)
    output = work / ("test.exe" if os.name == "nt" else "test")
    flags = ["-m32", "-std=gnu89", "-Werror", "-Wno-multichar"]
    if os.name == "nt":
        flags += ["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"]
    result = subprocess.run([cc, *flags, str(work / "test.c"), "-o", str(output)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


def asset():
    sprite = struct.pack("<hhI6f", 0, 0, 0, 0, .25, 0, .25, .5, .5)
    return bytearray(b"GCARCS1\0" + sprite * 31 + bytes(344064))


@pytest.mark.parametrize("case", ["valid", "missing", "truncated", "extra", "header", "index", "nan", "reversed", "registration", "texture-failure"])
def test_sprite_lifecycle(arcs_tool, tmp_path, case):
    payload = asset()
    if case == "truncated": payload.pop()
    if case == "extra": payload.append(0)
    if case == "header": payload[0] = 0
    if case == "index": struct.pack_into("<h", payload, 8, 2)
    if case == "nan": struct.pack_into("<f", payload, 16, float("nan"))
    if case == "reversed": struct.pack_into("<f", payload, 16, .5)
    if case == "registration": struct.pack_into("<f", payload, 32, 2)
    path = tmp_path / "arcs.bin"
    if case != "missing": path.write_bytes(payload)
    result = subprocess.run([str(arcs_tool), case, str(path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("payload", [b"", bytes(2048), struct.pack("<II", 0, 609) + bytes(2040)])
def test_extractor_rejects_bad_map(tmp_path, payload):
    path = tmp_path / "bad.map"
    path.write_bytes(payload)
    with pytest.raises(ValueError): extract(path)


def test_original_asset_optional():
    path = os.environ.get("SHIELD_ARCS_REFERENCE_MAP")
    if not path: pytest.skip("local Bigass reference map not supplied")
    import hashlib
    assert hashlib.sha256(extract(path)).hexdigest() == "cb28214c0e5b3dc35650abac199113c8332fdf9093f5c973de1d2ecc32cb3cbd"
