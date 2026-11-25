"""Exercise the desktop adaptive controller with separate presentation and GPU clocks."""
import subprocess

from tools.harness import read
from test_mcc_identity import compile_tool


def test_adaptive_shadows(tmp_path):
    controller = read('port/linux/src/modern_shadow_adaptive.inl')
    controller = '\n'.join(line for line in controller.splitlines() if not line.startswith('#include'))
    (tmp_path/'controller.inc').write_text(controller)
    source = tmp_path/'test.c'
    source.write_text(r'''
#include <assert.h>
#include <string.h>
typedef unsigned long long Uint64, GLuint64;
typedef unsigned int GLuint;
#define GL_TIMESTAMP 1
#define GL_QUERY_RESULT_AVAILABLE 2
#define GL_QUERY_RESULT 3
#define MODERN_SHADOW_LIGHTS 8
#define platform_log(...) ((void)0)
static int resolution, count, ceiling=32, target=240, available=1, reads;
static unsigned long revision;
static Uint64 clock_ns=1, gpu[2], render_ns;
static unsigned long config_changes(void) { return revision; }
static int config_boolean(const char *s) { return strstr(s,"resolution") ? resolution : count; }
static long config_integer(const char *s) { (void)s; return target; }
static long halo_shadow_resolution_scale(void) { return ceiling; }
static int xgpu_gl_ready(void) { return 1; }
static Uint64 SDL_GetTicksNS(void) { return clock_ns; }
static void glGenQueries(int n,GLuint *q) { assert(n==2);q[0]=1;q[1]=2; }
static void glDeleteQueries(int n,GLuint *q) { assert(n==2 && q[0]==1); }
static void glQueryCounter(GLuint q,int type) { assert(type==GL_TIMESTAMP);gpu[q-1]=clock_ns+(q==2?render_ns:0); }
static void glGetQueryObjectuiv(GLuint q,int key,GLuint *v) { assert(q==2 && key==GL_QUERY_RESULT_AVAILABLE); *v=available; }
static void glGetQueryObjectui64v(GLuint q,int key,GLuint64 *v) { assert(available && key==GL_QUERY_RESULT); *v=gpu[q-1];reads++; }
#include "controller.inc"
static void frames(int n,int fps,int work_us) {
    while(n--) {
        clock_ns+=1000000000ULL/fps; render_ns=work_us*1000ULL;
        assert(modern_adaptive_begin()==modern_adaptive_begin());
        modern_adaptive_present();
    }
}
int main(void) {
    const int rates[]={30,60,120,144,165,180,240}; int i, before;
    for(i=0;i<7;i++) assert(adaptive_target(rates[i])==rates[i]);
    assert(adaptive_target(0)==240 && adaptive_target(360)==240);
    frames(100,100,8000);assert(!reads && modern_shadow_pixels()==4096 && adaptive.slots==8);
    resolution=1;revision++;frames(500,120,7500);assert(modern_shadow_pixels()==128 && adaptive.slots==8);
    frames(1600,60,2500);assert(modern_shadow_pixels()==4096 && adaptive.slots==8);
    resolution=0;count=1;revision++;frames(1050,120,7500);
    assert(adaptive.slots==1 && modern_shadow_pixels()==4096);
    resolution=1;revision++;frames(280,120,7500);assert(adaptive.scale>1 && adaptive.slots==8);
    frames(350,120,7500);assert(adaptive.scale==1 && adaptive.slots<8);
    before=adaptive.slots;frames(260,60,2500);assert(adaptive.slots>before && adaptive.scale==1);
    available=0;before=reads;frames(400,60,2500);assert(before==reads);
    target=30;revision++;available=1;frames(600,35,20000);assert(adaptive.scale==32 && adaptive.slots==8);
    adaptive_reject_size();frames(180,60,2000);assert(adaptive.scale==16 && adaptive.ceiling==16);
    ceiling=4;frames(1,60,2000);assert(adaptive.scale==4 && adaptive.ceiling==4);
    adaptive_reset();assert(!adaptive.primary && !adaptive.pending && !adaptive.configured);
    return 0;
}
''')
    subprocess.run([str(compile_tool(tmp_path,[source]))],check=True)
