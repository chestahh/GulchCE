"""Test HUD cache priority, bounded recovery and frame diagnostics."""
from pathlib import Path
import re
import pytest
from harness import function
from test_android_terrain import run_c

ROOT=Path(__file__).resolve().parents[1]

def priority_program():
    source=(ROOT/'port/linux/game/android_texture_priority.c').read_text()
    source=re.sub(r'^#include .*$', '', source, flags=re.M)
    return r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define HALO_ANDROID 1
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 2304
#define NONE -1
#define FALSE 0
#define TRUE 1
#define MAX(a,b) ((a)>(b)?(a):(b))
struct bitmap_data {int flags;long pixels_size,hardware_size;int resident,touched,id;};
struct lruv_cache {long page_size_bits,page_count;};
static int ce=1,mcc,load_calls,touch_calls;
static char trace[1024],report[1024];
static int custom_edition_cache_tags_loaded(void) {return ce;}
static int mcc_cache_tags_loaded(void) {return mcc;}
static long rasterizer_xbox_bitmap_get_pixel_data_size(struct bitmap_data *b) {return b->hardware_size;}
static void * _texture_cache_bitmap_get_hardware_format(struct bitmap_data *b,int block,int load) {
 char item[20];assert(!block);
 snprintf(item,sizeof(item),"%c%d;",load?'L':'T',b->id);strcat(trace,item);
 if(!load) {touch_calls++;if(b->resident) {b->touched=1;return b;}return NULL;}
 load_calls++;b->resident=1;b->touched=1;return b;
}
void android_texture_test_log(const char *format,...) {
 va_list a;va_start(a,format);vsnprintf(report,sizeof(report),format,a);va_end(a);
}
''' + source + r'''
static struct bitmap_data a={128,16384,16384,0,0,1},b={128,16384,16384,1,0,2};
static struct lruv_cache cache={14,2304};
static void collect(struct bitmap_data *bitmap) {
 android_texture_priority_scope(1);android_texture_priority_note(bitmap);android_texture_priority_scope(0);
}
'''

@pytest.mark.parametrize('case',['scope','preload','expiry','forget','budget','recovery','first_miss','reset','reduced'])
def test_hud_priority_policy(tmp_path,case):
    cases={
        'scope':'''android_texture_priority_note(&a);assert(!hud_count);
ce=0;collect(&a);assert(!hud_count);ce=1;mcc=1;collect(&a);assert(!hud_count);
mcc=0;collect(&a);assert(hud_count==1);collect(&a);assert(hud_count==1 && hud_requests==2);
b.flags=0;collect(&b);assert(hud_count==1);
android_texture_priority_scope(1);assert(android_texture_priority_collecting());android_texture_priority_scope(0);
assert(!android_texture_priority_collecting());''',
        'preload':'''collect(&a);collect(&b);android_texture_priority_begin(&cache);
assert(!strcmp(trace,"T1;T2;L1;") && a.touched && b.touched);
assert(hud_preloads==2 && hud_ready==2 && load_calls==1 && hud_requests==2);
assert(!android_texture_priority_collecting());''',
        'expiry':'''collect(&a);android_texture_priority_begin(&cache);assert(hud_count==1);
android_texture_priority_begin(&cache);assert(hud_count==1);
android_texture_priority_begin(&cache);assert(!hud_count && !hud_bytes);
assert(touch_calls==2);''',
        'forget':'''collect(&a);collect(&b);android_texture_priority_forget(&a);
assert(hud_count==1 && hud_bytes==16384);android_texture_priority_begin(&cache);
assert(!strcmp(trace,"T2;") && !load_calls);
android_texture_priority_forget(&a);assert(hud_count==1);''',
        'budget':'''a.pixels_size=4*1024*1024-1;a.hardware_size=1;collect(&a);
assert(hud_bytes==4*1024*1024);collect(&b);assert(hud_rejected==1 && hud_count==1);
android_texture_priority_forget(&a);a.hardware_size=4*1024*1024+1;collect(&a);
assert(hud_rejected==2 && !hud_count);
a.pixels_size=1;a.hardware_size=16385;collect(&a);assert(hud_bytes==32768);
android_texture_priority_reset();a.hardware_size=a.pixels_size=-1;collect(&a);assert(!hud_count && hud_rejected==1);''',
        'recovery':'''assert(android_texture_priority_recover(&cache));
assert(!android_texture_priority_recover(&cache) && recovery_deferred==1);
android_texture_priority_begin(&cache);assert(android_texture_priority_recover(&cache));
assert(recovery_attempts==2);mcc=1;assert(!android_texture_priority_recover(&cache));
mcc=0;ce=0;assert(!android_texture_priority_recover(&cache));''',
        'first_miss':'''android_texture_priority_scope(1);android_texture_priority_note(&a);
android_texture_priority_result(&a,0);android_texture_priority_scope(0);
assert(hud_missing==1 && hud_count==1);
android_texture_priority_begin(&cache);assert(a.resident && hud_ready==1);
android_texture_priority_scope(1);android_texture_priority_note(&a);android_texture_priority_result(&a,1);
assert(hud_missing==1 && hud_requests==2);''',
        'reset':'''collect(&a);android_texture_priority_recover(&cache);
android_texture_priority_reset();assert(!hud_count && !hud_bytes && !hud_requests && !recovery_used);
android_texture_priority_begin(&cache);assert(!touch_calls && !load_calls);
assert(android_texture_priority_recover(&cache));''',
        'reduced':'''collect(&a);cache.page_count=200;android_texture_priority_begin(&cache);
assert(!load_calls && !touch_calls && !android_texture_priority_recover(&cache));
cache.page_count=2304;android_texture_priority_begin(&cache);assert(load_calls==1);'''
    }
    run_c(tmp_path,priority_program()+'int main(void) {\n'+cases[case]+'\nreturn 0;}\n')

def test_real_engine_hooks_and_isolation():
    source=(ROOT/'source/cache/xbox_texture_cache.c').read_text()
    idle=function(source,'texture_cache_idle')
    assert idle.index('lruv_idle') < idle.index('android_texture_priority_begin')
    load=function(source,'texture_cache_start_loading_bitmap')
    assert load.index('lruv_block_new') < load.index('android_texture_priority_recover')
    get=function(source,'_texture_cache_bitmap_get_hardware_format')
    assert get.index('mcc_texture_cache_get') < get.index('android_texture_priority_note')
    assert 'android_texture_priority_collecting()' in get
    for name in ('texture_cache_flush','texture_cache_open','texture_cache_delete'):
        assert 'android_texture_priority_reset' in function(source,name)
    assert 'android_texture_priority_forget(bitmap)' in function(source,'texture_cache_bitmap_delete')
    hud=function((ROOT/'source/interface/hud.c').read_text(),'hud_draw_screen')
    assert hud.index('android_texture_priority_scope(TRUE)') < hud.index('hud_render_weapon_interface')
    assert hud.index('android_texture_priority_scope(FALSE)') > hud.index('temporary_hud_draw()')
    stripped=re.sub(r'#ifdef HALO_ANDROID\n.*?#endif','',source,flags=re.S)
    assert 'android_texture_priority_' not in stripped
    # The unsuccessful GPU-rekey experiment is not part of this comparison.
    gpu=(ROOT/'port/linux/src/xbox_textures.c').read_text()
    move=function(gpu,'android_texture_move_texels')
    assert 'memmove' in move and 'texture_buckets' not in move

def test_priority_survives_real_lru_pressure(tmp_path):
    from test_android_texture_compact import compact_program
    code=compact_program()
    code=code.replace('struct bitmap_data {byte *base_address;};',
                      'struct bitmap_data {byte *base_address;long pixels_size;int flags;};')
    source=(ROOT/'port/linux/game/android_texture_priority.c').read_text()
    source=re.sub(r'^#include .*$', '', source, flags=re.M)
    code+=r'''
#define HALO_ANDROID 1
#define MAX(a,b) ((a)>(b)?(a):(b))
static int custom_edition_cache_tags_loaded(void) {return 1;}
static int mcc_cache_tags_loaded(void) {return 0;}
static long rasterizer_xbox_bitmap_get_pixel_data_size(struct bitmap_data *b) {return b->pixels_size;}
static void android_texture_test_log(const char *f,...) {}
static void *_texture_cache_bitmap_get_hardware_format(struct bitmap_data *b,int block,int load) {
 long i=b-bitmaps;assert(i>=0 && i<8);
 if(blocks.active[i] && !deleted[i]) {blocks.items[i].last_used_tick=cache.tick;return &entries[i].hardware_format;}
 assert(!load);return NULL;
}
'''
    # Same policy compiled for a small real allocator fixture; no policy logic copied.
    code+='#undef HALO_PORT_TEXTURE_CACHE_PAGE_COUNT\n#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 16\n'+source
    code+=r'''
int main(void) {
 long id;
 setup();cache.tick++;
 bitmaps[0].pixels_size=32768;bitmaps[0].flags=128;
 android_texture_priority_scope(1);android_texture_priority_note(&bitmaps[0]);android_texture_priority_scope(0);
 android_texture_priority_begin(&cache);
 id=lruv_block_new(&cache,14*16384);
 assert(id!=NONE && !deleted[0] && blocks.active[0]);
 assert(blocks.items[0].last_used_tick==cache.tick && pixels[0]==1);
 assert(hud_ready==1 && hud_missing==0);
 return 0;
}
'''
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
 android_texture_test_pressure(2,1,1,1,4);
 android_texture_perf_frame(2,60);
 if(!strstr(report,"frame=1 interval_ms=50 uploads=3 upload_CPU_ms=12 retries=2 failed=1 fallback=1 recovered=1 recovery_ms=4"))return 1;
 length=0;report[0]=0;android_texture_perf_frame(3,110);if(length)return 2;
 android_texture_perf_frame(4,10010);
 if(!strstr(report,"frames=3 interval_sum_ms=10000 max_interval_ms=9900 intervals_over_33ms=3"))return 3;
 if(android_texture_perf_frames || android_texture_perf_window_ms || android_texture_perf_max)return 4;
 return 0;
}
'''
    run_c(tmp_path,code)
