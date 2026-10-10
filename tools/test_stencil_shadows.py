"""Boundary and state-restoration tests for the optional desktop shadow pass."""
from pathlib import Path
import shutil
import subprocess
import sys

import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]
SHADOWS = (ROOT / 'port/linux/game/stencil_shadows.c').read_text()
BACKEND = (ROOT / 'port/linux/src/d3d8_gl.c').read_text()


def run_c(tmp_path, code):
    compiler = shutil.which('clang')
    if not compiler:
        pytest.skip('clang required')
    source = tmp_path / 'shadow.c'
    source.write_text(code)
    binary = tmp_path / ('shadow.exe' if sys.platform == 'win32' else 'shadow')
    target = ['--target=i686-pc-windows-msvc', '-fuse-ld=lld'] if sys.platform == 'win32' else []
    result = subprocess.run([compiler, *target, str(source), '-o', str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert subprocess.run([str(binary)], timeout=10).returncode == 0


def test_volume_capacity_and_allocation_failure(tmp_path):
    run_c(tmp_path, r'''
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define STENCIL_SHADOW_BUFFER_MAXIMUM 900000ul
struct stencil_shadow_buffer {float *positions; unsigned long count, capacity;};
static int fail, calls;
static void *allocate(void *p, size_t size) {calls++; return fail ? NULL : realloc(p,size);}
#define realloc allocate
''' + function(SHADOWS, 'stencil_shadow_reserve') + r'''
int main(void) {
    struct stencil_shadow_buffer b={0};
    float *old;
    assert(stencil_shadow_reserve(&b,3)); old=b.positions;
    b.positions[0]=42; b.count=b.capacity; fail=1;
    assert(!stencil_shadow_reserve(&b,3));
    assert(b.positions==old && b.positions[0]==42);
    fail=0; b.count=0;
    assert(stencil_shadow_reserve(&b,STENCIL_SHADOW_BUFFER_MAXIMUM));
    assert(b.capacity==STENCIL_SHADOW_BUFFER_MAXIMUM);
    calls=0;b.count=STENCIL_SHADOW_BUFFER_MAXIMUM;
    assert(!stencil_shadow_reserve(&b,1) && calls==0);
    b.count=ULONG_MAX;
    assert(!stencil_shadow_reserve(&b,3) && calls==0);
    b.count=1;
    assert(!stencil_shadow_reserve(&b,ULONG_MAX) && calls==0);
    free(b.positions);return 0;
}
''')


def test_backend_restores_shaders_constants_and_attributes(tmp_path):
    run_c(tmp_path, r'''
#include <string.h>
#include <assert.h>
typedef unsigned long DWORD;
#define D3DRS_MAX 128
#define XGPU_VERTEX_CONSTANT_COUNT 192
#define XGPU_VERTEX_ATTRIBUTE_COUNT 16
struct vertex_shader_object {int id;};
static DWORD D3D__RenderState[D3DRS_MAX];
static struct {
    struct vertex_shader_object *vertex_shader,*program_slots[4];
    unsigned long program_address;
    float constants[XGPU_VERTEX_CONSTANT_COUNT][4];
    float attributes[XGPU_VERTEX_ATTRIBUTE_COUNT][4];
} device;
static int stores;
static void constants_store(unsigned long first,const void *data,unsigned long count) {
    assert(first==0 && count==XGPU_VERTEX_CONSTANT_COUNT);
    stores++;memcpy(device.constants,data,sizeof(device.constants));
}
''' + function(BACKEND, 'd3d8_gl_shadow_state') + r'''
int main(void) {
    struct vertex_shader_object original={1}, replacement={2};
    unsigned long i;
    device.vertex_shader=&original;device.program_slots[0]=&original;device.program_address=3;
    for(i=0;i<D3DRS_MAX;i++) D3D__RenderState[i]=i+1;
    device.constants[28][1]=123;device.attributes[9][0]=456;
    d3d8_gl_shadow_state(0);
    memset(D3D__RenderState,0,sizeof(D3D__RenderState));
    device.vertex_shader=&replacement;device.program_slots[0]=&replacement;device.program_address=0;
    memset(device.constants,0,sizeof(device.constants));memset(device.attributes,0,sizeof(device.attributes));
    d3d8_gl_shadow_state(1);
    assert(device.vertex_shader==&original && device.program_slots[0]==&original && device.program_address==3);
    for(i=0;i<D3DRS_MAX;i++) assert(D3D__RenderState[i]==i+1);
    assert(device.constants[28][1]==123 && device.attributes[9][0]==456 && stores==1);
    return 0;
}
''')


@pytest.mark.parametrize('platform', ['HALO_ARM64_GUEST', 'HALO_WEB', 'HALO_GLES'])
def test_non_desktop_has_no_renderer_imports(tmp_path, platform):
    prefix = SHADOWS[:SHADOWS.index('#else') + len('#else')]
    # Compile the real platform guard and stubs without any engine/backend imports.
    prefix = prefix.replace('#include "stencil_shadows.h"', '')
    run_c(tmp_path, '#define '+platform+' 1\n'+prefix+'\n#endif\nint main(void) {stencil_shadows_render();stencil_shadows_reset();return 0;}')


def test_shadow_mesh_owns_transient_reader_data(tmp_path):
    edge = SHADOWS[SHADOWS.index('struct stencil_shadow_edge\n'):SHADOWS.index('/* ---------- globals */')]
    run_c(tmp_path, r'''
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef unsigned char byte;
typedef struct {float x,y,z;} real_point3d;
#define MAXIMUM_REGIONS_PER_OBJECT 8
#define STENCIL_SHADOW_MAXIMUM_MESHES 16
#define OBJECT_MESH_MAXIMUM_VERTICES 16384
#define OBJECT_MESH_MAXIMUM_TRIANGLES 24576
#define OBJECT_MESH_MAXIMUM_NODES 64
#define NONE (-1)
struct object_mesh_vertex {real_point3d position;};
struct object_mesh {long model_index,vertex_count,triangle_count,node_count;struct object_mesh_vertex *vertices;unsigned short (*triangles)[3];};
struct object_datum {long definition_index;struct {byte region_permutations[8];} object;};
struct object_definition {struct {struct {long index;} model;} object;};
static struct object_datum obj;
static struct object_definition def;
static struct object_mesh transient;
static int reads,fail_build;
static struct object_datum *object_try_and_get(long i) {(void)i;return &obj;}
static struct object_definition *object_definition_get(long i) {(void)i;return &def;}
static struct object_mesh *object_mesh_get(long i) {(void)i;reads++;transient.model_index=def.object.model.index;return &transient;}
''' + edge + r'''
static struct stencil_shadow_mesh stencil_shadow_meshes[16];
static unsigned long stencil_shadow_frame=1;
static int stencil_shadow_build(struct stencil_shadow_mesh *entry,struct object_mesh const *mesh) {
    if(fail_build)return 0;
    entry->mesh=mesh;return 1;
}
''' + function(SHADOWS, 'stencil_shadow_surface_get') + r'''
int main(void) {
    struct object_mesh_vertex vertices[3]={{{0,0,0}},{{1,0,0}},{{0,1,0}}};
    unsigned short triangles[1][3]={{0,1,2}};
    struct stencil_shadow_mesh *first,*second;
    int i;
    transient.vertices=vertices;transient.triangles=triangles;transient.vertex_count=3;transient.triangle_count=1;transient.node_count=1;
    def.object.model.index=1;
    first=stencil_shadow_surface_get(0);assert(first && reads==1);
    assert(first->surface.vertices!=vertices && first->surface.triangles!=triangles);
    vertices[1].position.x=5;def.object.model.index=2;stencil_shadow_frame++;
    second=stencil_shadow_surface_get(0);assert(second && second!=first && reads==2);
    assert(first->surface.vertices[1].position.x==1);
    def.object.model.index=1;assert(stencil_shadow_surface_get(0)==first && reads==2);
    obj.object.region_permutations[7]=1;
    assert(stencil_shadow_surface_get(0)!=first && reads==3);
    def.object.model.index=3;triangles[0][2]=999;
    assert(!stencil_shadow_surface_get(0));
    triangles[0][2]=2;fail_build=1;assert(!stencil_shadow_surface_get(0));
    fail_build=0;assert(stencil_shadow_surface_get(0));
    for(i=0;i<16;i++){free(stencil_shadow_meshes[i].surface.vertices);free(stencil_shadow_meshes[i].surface.triangles);}
    return 0;
}
''')


def test_volume_draw_ignores_invalidated_winding_and_oversized_upload(tmp_path):
    run_c(tmp_path, r'''
#include <string.h>
#include <assert.h>
typedef unsigned int GLenum;
typedef int GLsizei;
#define TRUE 1
#define FALSE 0
#define GL_FLOAT 1
#define GL_FALSE 0
#define GL_CCW 2
#define GL_CW 3
#define GL_FRONT 4
#define GL_BACK 5
#define GL_KEEP 6
#define GL_DECR_WRAP 7
#define GL_INCR_WRAP 8
#define GL_TRIANGLES 9
#define STREAM_BUFFER_SIZE (32UL*1024*1024)
struct vertex_array_entry {int dummy;};
struct vertex_layout {int dummy;};
static struct {unsigned long stream_buffer;} device;
static struct {GLenum front_face,stencil_operations[3];} gl_state;
static int uploads,draws;
static int prepare_draw(int a){(void)a;return 1;}
static unsigned long stream_upload(const void *p,unsigned long n){(void)p;assert(n<=STREAM_BUFFER_SIZE);uploads++;return 0;}
static void layout_attribute(struct vertex_layout *p,int a,int b,int c,int d,int e,int f,int g){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;}
static struct vertex_array_entry *vertex_array_get(const struct vertex_layout *p){static struct vertex_array_entry e;(void)p;return &e;}
static void state_vertex_array(void *p){(void)p;}
static void state_vertex_buffer(int a,unsigned long b,unsigned long c,GLsizei d){(void)a;(void)b;(void)c;(void)d;}
static void glFrontFace(GLenum v){assert(v==GL_CCW || v==GL_CW);}
static void glStencilOpSeparate(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void glStencilOp(int a,int b,int c){(void)a;(void)b;(void)c;}
static void glDrawArrays(int a,int b,int c){(void)a;(void)b;(void)c;draws++;}
static void gl_check_errors(const char *p){(void)p;}
''' + function(BACKEND, 'd3d8_gl_draw_shadow_volume') + r'''
int main(void) {
    float triangle[9]={0};
    gl_state.front_face=(GLenum)-1;
    d3d8_gl_draw_shadow_volume(triangle,3,1);
    gl_state.front_face=GL_CW;
    d3d8_gl_draw_shadow_volume(triangle,3,0);
    assert(uploads==2 && draws==2);
    d3d8_gl_draw_shadow_volume(triangle,STREAM_BUFFER_SIZE,0);
    assert(uploads==2 && draws==2);return 0;
}
''')


def test_reset_releases_every_owned_allocation(tmp_path):
    run_c(tmp_path, r'''
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define STENCIL_SHADOW_MAXIMUM_MESHES 16
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define NONE (-1)
#define csmemset memset
static struct {struct {void *vertices,*triangles;} surface;unsigned long last_used;void *edges,*welded,*mesh;} stencil_shadow_meshes[16];
static struct buffer {void *positions;unsigned long count,capacity;} stencil_shadow_depth_fail,stencil_shadow_depth_pass;
static struct {struct buffer sides,caps;long object_index;} stencil_shadow_cache[20];
static void *stencil_shadow_world,*stencil_shadow_lit;
static long stencil_shadow_world_capacity,stencil_shadow_lit_capacity;
static unsigned long stencil_shadow_frame;
static int releases;
static void release(void *p) {if(p)releases++;free(p);}
#define free release
''' + function(SHADOWS[SHADOWS.index('#else'):], 'stencil_shadows_reset') + r'''
int main(void) {
    int i;
    for(i=0;i<16;i++){
        stencil_shadow_meshes[i].surface.vertices=malloc(1);stencil_shadow_meshes[i].surface.triangles=malloc(1);
        stencil_shadow_meshes[i].edges=malloc(1);stencil_shadow_meshes[i].welded=malloc(1);
        stencil_shadow_meshes[i].mesh=(void *)1;stencil_shadow_meshes[i].last_used=9;
    }
    for(i=0;i<20;i++){stencil_shadow_cache[i].sides.positions=malloc(1);stencil_shadow_cache[i].caps.positions=malloc(1);}
    stencil_shadow_world=malloc(1);stencil_shadow_lit=malloc(1);
    stencil_shadow_depth_fail.positions=malloc(1);stencil_shadow_depth_pass.positions=malloc(1);
    stencil_shadow_frame=9;stencil_shadow_world_capacity=stencil_shadow_lit_capacity=100;
    stencil_shadows_reset();assert(releases==108);
    assert(!stencil_shadow_world && !stencil_shadow_lit && !stencil_shadow_frame);
    assert(!stencil_shadow_world_capacity && !stencil_shadow_lit_capacity);
    for(i=0;i<16;i++) assert(!stencil_shadow_meshes[i].mesh && !stencil_shadow_meshes[i].last_used);
    stencil_shadows_reset();assert(releases==108);return 0;
}
''')
