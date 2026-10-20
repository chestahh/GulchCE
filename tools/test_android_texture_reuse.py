"""Exercise relocation through the real GPU-cache lookup, including stale keys."""
from pathlib import Path
import re
import pytest
from harness import function
from test_android_terrain import run_c

ROOT = Path(__file__).resolve().parents[1]


def program():
    source=(ROOT/'port/linux/src/xbox_textures.c').read_text()
    # Reuse the existing cache test's dependency stubs, not a copy of its cache logic.
    test=(ROOT/'tools/test_android_texture_cache.py').read_text()
    code='#define HALO_ANDROID 1\n'+test.split("code = defines + r'''",1)[1].split("'''",1)[0]
    code=code.replace('static BOOL config_boolean(const char *s) { return FALSE; }',
        'static BOOL no_cache; static BOOL config_boolean(const char *s) { return no_cache; }')
    code=code.replace('static unsigned long memory_watch_generation(unsigned long a,unsigned long s) { return generation; }',r'''
static unsigned char pixels[4*16384];
static unsigned long generations[4]={1,1,1,1};
static unsigned long memory_watch_generation(unsigned long a,unsigned long s) {
 unsigned long first=(a-(unsigned long)pixels)/16384,last=(a+s-1-(unsigned long)pixels)/16384,g=0,i;
 if(first>=4 || last>=4) abort();for(i=first;i<=last;i++) if(generations[i]>g)g=generations[i];return g;
}
static void memory_watch_prepare_write(void *p,unsigned long s) { }
static void memory_watch_forget(void *p,unsigned long s) {
 unsigned long first=((unsigned long)p-(unsigned long)pixels)/16384,last=((unsigned long)p+s-1-(unsigned long)pixels)/16384,i;
 serial++;for(i=first;i<=last;i++)generations[i]=serial;
}
#define PLATFORM_VIRTUAL_TO_PHYSICAL(x) ((DWORD)(x))
#define _custom_edition_channels_xbox 0
static unsigned long android_texture_reuse_moves,android_texture_reuse_kept;
static Uint64 android_texture_reuse_bytes;
static void halo_custom_edition_texels_channels(const void *p,unsigned char c) { }
static unsigned long deletes,invalidations;
''')
    code=code.replace('static void glDeleteTextures(int n,GLuint *t) {}','static void glDeleteTextures(int n,GLuint *t) { deletes++; }')
    code=code.replace('static void xgpu_gl_state_invalidate(void) {}','static void xgpu_gl_state_invalidate(void) { invalidations++; }')
    code+=source[source.index('struct texture_entry\n'):source.index('/* (every bit of the three mixed')]
    for f in ('bucket_index','palette_hash','android_texture_move_texels'):
        code+=function(source,f)+'\n'
    code+=r'''
static GLuint texture_entry_result(struct texture_entry *e,GLenum *t,struct xgpu_texture_description *d) {
 *t=e->target;*d=e->description;return e->texture;
}
static Uint64 clock_ms;
static Uint64 SDL_GetTicks(void) {return clock_ms;}
static unsigned long hud_hires_settings_changed(void) {return 0;}
static BOOL hud_hires_bitmap_affected(unsigned long a,long w,long h,unsigned long c) {return FALSE;}
static void android_texture_test_begin_frame(unsigned long f) { }
'''+function(source,'xgpu_texture_get')+'\n'+function(source,'xgpu_texture_cache_begin_frame')+r'''
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static struct texture_entry *find(DWORD *r) {
 struct texture_entry *e;for(e=texture_buckets[bucket_index(r[1],r[3],r[4])];e;e=e->next)
 if(e->data==r[1] && e->format_word==r[3] && e->size_word==r[4])return e;return NULL;
}
'''
    return code


@pytest.mark.parametrize('scenario',['keep','destination_collision','stale','palette','override','upscale','no_cache','chain','unrelated_key','extent','mcc','overlap','sweep','zero_generation'])
def test_reuse(tmp_path,scenario):
    cases={
        'keep': '''android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original && uploads==1 && android_texture_reuse_kept==1);
CHECK(pixels[0]==42 && find(r)->address==(unsigned long)pixels);
/* A later write must invalidate the retained GPU copy, even in this frame. */
pixels[0]=43;memory_watch_forget(pixels,1);
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original && uploads==2);''',
        'destination_collision': '''other[1]=(DWORD)pixels;xgpu_texture_get(other,NULL,&target,&d);CHECK(uploads==2);
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original); /* Fill both recent slots. */
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(deletes==1 && invalidations==1 && xgpu_texture_get(r,NULL,&target,&d)==original && uploads==2);
/* The old address must not find the rekeyed entry through recent_textures. */
other[1]=(DWORD)(pixels+16384);CHECK(xgpu_texture_get(other,NULL,&target,&d)!=original && uploads==3);''',
        'stale': '''pixels[16384]=55;memory_watch_forget(pixels+16384,1);
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(xgpu_texture_get(r,NULL,&target,&d)!=original && uploads==2 && !android_texture_reuse_kept);''',
        'palette': '''r[3]=11;xgpu_texture_get(r,NULL,&target,&d);
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==3 && !android_texture_reuse_kept);''',
        'override': '''find(r)->override=7;android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==2 && !android_texture_reuse_kept);''',
        'upscale': '''find(r)->hud_upscale=7;android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==2 && !android_texture_reuse_kept);''',
        'no_cache': '''no_cache=TRUE;android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==2 && !android_texture_reuse_kept);''',
        'chain': '''other[1]=(DWORD)(pixels+32768);second=xgpu_texture_get(other,NULL,&target,&d);
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
android_texture_move_texels(pixels+16384,pixels+32768,16384,other[3],other[4]);other[1]=(DWORD)(pixels+16384);
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original && xgpu_texture_get(other,NULL,&target,&d)==second);
CHECK(uploads==2 && android_texture_reuse_kept==2);
/* Same texture moved a second time, without drawing in between. */
android_texture_move_texels(pixels+32768,pixels,16384,r[3],r[4]);r[1]=(DWORD)(pixels+32768);
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original && uploads==2);''',
        'unrelated_key': '''other[1]=(DWORD)pixels;other[4]=1;second=xgpu_texture_get(other,NULL,&target,&d);
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(!deletes && xgpu_texture_get(r,NULL,&target,&d)==original && uploads==2);
CHECK(xgpu_texture_get(other,NULL,&target,&d)==second && uploads==3);''',
        'extent': '''android_texture_move_texels(pixels,pixels+16384,512,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==2 && !android_texture_reuse_kept);''',
        'mcc': '''mcc=TRUE;CHECK(xgpu_texture_get(r,NULL,&target,&d)==999 && uploads==1 && !android_texture_reuse_kept);''',
        'overlap': '''memset(pixels+32768,99,16384);
android_texture_move_texels(pixels,pixels+16384,32768,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(xgpu_texture_get(r,NULL,&target,&d)==original && uploads==1);
CHECK(pixels[0]==42 && pixels[16384]==99);''',
        'sweep': '''android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
CHECK(find(r)->last_used_frame==1);texture_frame=50;texture_sweep_count=3;texture_sweep_frames[0]=2;clock_ms=40000;
xgpu_texture_cache_begin_frame();CHECK(!find(r) && deletes==1);
CHECK(xgpu_texture_get(r,NULL,&target,&d)!=original && uploads==2);''',
        'zero_generation': '''generations[1]=0;
android_texture_move_texels(pixels,pixels+16384,16384,r[3],r[4]);r[1]=(DWORD)pixels;
xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==2 && !android_texture_reuse_kept);'''
    }
    code=program()+r'''
int main(void) {
 DWORD r[5]={0,0,0,0,0},other[5]={0,0,0,0,0};GLenum target;struct xgpu_texture_description d;GLuint original,second;
 memset(pixels+16384,42,16384);r[1]=(DWORD)(pixels+16384);texture_frame=1;
 original=xgpu_texture_get(r,NULL,&target,&d);CHECK(uploads==1);
'''+cases[scenario]+'\nreturn 0;}\n'
    run_c(tmp_path,code)


def test_frame_correlation(tmp_path):
    header=(ROOT/'port/android/guest/texture_perf_test.h').read_text()
    code=r'''
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
typedef unsigned long long Uint64;
typedef int BOOL;
#define TRUE 1
static unsigned long android_texture_test_uploads;
static Uint64 android_texture_test_upload_ms;
static char report[8192];static int length;
static void android_texture_test_log(const char *f,...) {
 va_list v;va_start(v,f);length+=vsnprintf(report+length,sizeof(report)-length,f,v);va_end(v);
}
'''+header+r'''
int main(void) {
 android_texture_perf_frame(1,10);
 android_texture_test_uploads=3;android_texture_test_upload_ms=12;
 android_texture_reuse_kept=5;android_texture_test_pressure(2,1,1,1,4);
 android_texture_perf_frame(2,60);
 if(!strstr(report,"frame=1 interval_ms=50 uploads=3 upload_CPU_ms=12 retries=2 failed=1 fallback=1 recovered=1 recovery_ms=4 gpu_copies_kept=5"))return 1;
 length=0;report[0]=0;android_texture_perf_frame(3,110);if(length)return 2;
 android_texture_perf_frame(4,10010);
 if(!strstr(report,"frames=3 interval_sum_ms=10000 max_interval_ms=9900 intervals_over_33ms=3"))return 3;
 if(android_texture_perf_frames || android_texture_perf_window_ms || android_texture_perf_max)return 4;
 if(!strstr(report,"retries=2 failed=1 fallback=1 recovered=1 recovery_ms=4"))return 5;
 return 0;
}
'''
    run_c(tmp_path,code)
