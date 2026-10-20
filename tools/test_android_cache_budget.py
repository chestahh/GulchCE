"""Exercise diagnostic counts, pressure snapshots and the real window allocator."""
from pathlib import Path
import re
import pytest
from harness import function
from test_android_terrain import run_c

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('define,expected', [
    ('HALO_ARM64_GUEST', 36), ('HALO_WEB', 22), ('HALO_WINDOWS', 256), ('', 256)])
def test_platform_budget(tmp_path, define, expected):
    header = ROOT / 'port/linux/include/halo_port_capacity.h'
    code = (f'#define {define} 1\n' if define else '') + f'#include "{header.as_posix()}"\n'
    code += f'''int main(void) {{
 if (HALO_PORT_TEXTURE_CACHE_SIZE != {expected}UL*1024*1024) return 1;
 if (HALO_PORT_GAME_STATE_BASE_ADDRESS != 0x81a00000UL || HALO_PORT_GAME_STATE_SIZE != 0x1400000UL) return 2;
 return 0;
}}'''
    run_c(tmp_path, code)


def test_pressure_accounting_and_lifecycle(tmp_path):
    source = (ROOT/'port/linux/game/android_cache_test.c').read_text()
    header = (ROOT/'port/linux/game/android_cache_test.h').read_text()
    # Compile the entire diagnostic implementation with only its dependencies stubbed.
    source = re.sub(r'^#include .*$', '', source, flags=re.M)
    code = (ROOT/'port/linux/game/android_texture_compact.h').read_text() + r'''
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define HALO_ANDROID 1
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 8
#define NONE -1
typedef unsigned char byte;
struct lruv_cache { long page_count, page_size_bits; };
struct bitmap_data { short width,height,depth,format,mipmap_count; unsigned short flags; long tag_index,pixels_size,cache_block_index; };
static unsigned long now;
static int snapshots, lines, ce=1, mcc;
static char last[1024], detail[1024];
static unsigned long system_milliseconds(void) { return now; }
static int custom_edition_cache_tags_loaded(void) { return ce; }
static int mcc_cache_tags_loaded(void) { return mcc; }
static const char *tag_get_name(long i) { return "terrain"; }
static void lruv_cache_get_page_usage(struct lruv_cache *c, byte *u) {
 byte values[8]={3,1,0,9,0,0,1,0}; memcpy(u,values,8); snapshots++;
}
void android_cache_test_memory(unsigned long *a,unsigned long *b) { *a=100;*b=50; }
void android_texture_test_pressure(unsigned long a,unsigned long b,unsigned long c,unsigned long d,unsigned long e) {}
void android_texture_test_log(const char *f,...) {
 va_list v; va_start(v,f);vsnprintf(last,sizeof(last),f,v);va_end(v);lines++;
 if(strstr(f,"bitmap_cache_event")) strcpy(detail,last);
}
''' + header + source + r'''
int main(void) {
 struct lruv_cache cache={8,14}; struct bitmap_data b={1024,1024,1,15,8,2,7,1398096,-1}; int before;
 struct android_texture_compact_result moved={20,3,2,1};
 android_cache_test_reset(&cache,"open");
 android_cache_test_tick(&cache);
 if(!strstr(last,"occupied=4 locked=2 largest_free_pages=2 largest_evictable_span_pages=4")) return 1;
 if(!strstr(last,"family=ce") || !strstr(last,"physical_free=100 physical_largest=50")) return 2;
 before=snapshots;android_cache_test_tick(&cache);if(snapshots!=before) return 3;
 android_cache_test_record(&cache,&b,_android_cache_test_allocation_failed,1398096);
 if(!strstr(detail,"event=allocation_failed requested=1398096 bitmap=terrain")) return 4;
 before=lines;android_cache_test_record(&cache,&b,_android_cache_test_allocation_failed,1398096);
 if(lines!=before || cache_events[2]!=2) return 5;
 android_cache_test_record(&cache,&b,_android_cache_test_rejected,0);
 android_cache_test_record(&cache,&b,_android_cache_test_fallback,1398096);
 android_cache_test_record(&cache,&b,_android_cache_test_allocated,1398096);
 if(cache_events[0]!=1 || cache_events[1]!=1 || cache_events[3]!=1) return 6;
 now=5000;mcc=1;android_cache_test_tick(&cache);if(!strstr(last,"family=mcc")) return 7;
 android_cache_test_reset(&cache,"close");if(cache_events[0] || cache_events[2] || cache_next_summary) return 8;
 mcc=ce=0;android_cache_test_tick(&cache);if(!strstr(last,"family=xbox/menu")) return 9;
 cache.page_count=9;before=snapshots;android_cache_test_snapshot(&cache,"bad");if(snapshots!=before) return 10;
 cache.page_count=8;android_cache_test_record(&cache,&b,_android_cache_test_retry,1398096);
 android_cache_test_compaction(&moved,7,1);
 if(compact_recovered!=1 || compact_pages!=20 || compact_evicted!=2 || compact_ms!=7) return 11;
 before=lines;android_cache_test_compaction(&moved,5,1);
 if(lines!=before || compact_recovered!=2 || compact_ms!=12) return 12;
 moved.status=2;android_cache_test_compaction(&moved,0,0);
 moved.status=0;android_cache_test_compaction(&moved,0,0);
 if(compact_busy!=1 || compact_capacity!=1) return 13;
 android_cache_test_reset(&cache,"close");
 if(compact_recovered || compact_pages || compact_evicted || compact_ms || compact_busy || compact_capacity || cache_events[4]) return 14;
 return 0;
}
'''
    run_c(tmp_path, code)


def test_actual_window_allocator_headroom(tmp_path):
    source = (ROOT/'port/linux/src/xbox_memory.c').read_text()
    capacity = ROOT/'port/linux/include/halo_port_capacity.h'
    abi = ROOT/'port/android/include/halo_android_abi.h'
    code = f'#define HALO_ARM64_GUEST 1\n#include "{capacity.as_posix()}"\n#include "{abi.as_posix()}"\n'
    code += r'''
#include <stddef.h>
#include <string.h>
#define TRUE 1
#define FALSE 0
typedef int BOOL;
typedef unsigned long DWORD;
#define PAGE_SIZE_BYTES 4096UL
#define PLATFORM_CONTIGUOUS_BASE HALO_GUEST_WINDOW_BASE
#define PLATFORM_CONTIGUOUS_SIZE HALO_GUEST_WINDOW_SIZE
#define PLATFORM_ANY_PHYSICAL_ADDRESS 0xffffffffUL
#define CONTIGUOUS_PAGE_COUNT (PLATFORM_CONTIGUOUS_SIZE/PAGE_SIZE_BYTES)
#define PAGE_WRITECOMBINE 0x400
#define PAGE_NOCACHE 0x200
#define PAGE_READWRITE 4
#define MAP_PRIVATE 1
#define MAP_ANONYMOUS 2
#define MAP_FIXED 4
static DWORD page_protection[CONTIGUOUS_PAGE_COUNT];
static unsigned long block_page_count[CONTIGUOUS_PAGE_COUNT];
static int arena_lock,arena_reserved=1,locks;
static void pthread_mutex_lock(int *p) { locks++; }
static void pthread_mutex_unlock(int *p) { locks--; }
static void memory_watch_forget(void *p,unsigned long s) { }
static int protection_to_host(DWORD p) { return 0; }
static void *mmap(void *p,unsigned long n,int prot,int flags,int fd,int off) { return p; }
'''
    for name in ('pages_free','platform_contiguous_alloc','android_cache_test_memory'):
        code += function(source,name)
    code += r'''
int main(void) {
 unsigned long free_bytes,largest;void *texture;
 if(HALO_GUEST_WINDOW_BASE+HALO_GUEST_WINDOW_SIZE!=HALO_GUEST_IMAGE_BASE) return 1;
 if(!platform_contiguous_alloc(HALO_PORT_GAME_STATE_SIZE,0,HALO_PORT_GAME_STATE_BASE_ADDRESS-PLATFORM_CONTIGUOUS_BASE,4)) return 2;
 if(!platform_contiguous_alloc(0x1600000,0,0x3a6000,4)) return 3;
 texture=platform_contiguous_alloc(HALO_PORT_TEXTURE_CACHE_SIZE,0,PLATFORM_ANY_PHYSICAL_ADDRESS,4);
 if(texture!=(void *)0x85c00000UL) return 4;
 if(!platform_contiguous_alloc(HALO_PORT_SOUND_CACHE_SIZE,0,PLATFORM_ANY_PHYSICAL_ADDRESS,4)) return 5;
 android_cache_test_memory(&free_bytes,&largest);
 if(largest!=42UL*1024*1024 || free_bytes<largest || locks) return 6;
 if(platform_contiguous_alloc(43UL*1024*1024,0,PLATFORM_ANY_PHYSICAL_ADDRESS,4)) return 7;
 if(!platform_contiguous_alloc(40UL*1024*1024,0,PLATFORM_ANY_PHYSICAL_ADDRESS,4)) return 8;
 arena_reserved=0;android_cache_test_memory(&free_bytes,&largest);
 if(free_bytes || largest || locks) return 9;
 return 0;
}
'''
    run_c(tmp_path, code)


def test_hooks_preserve_mcc_and_record_real_failure():
    source = (ROOT/'source/cache/xbox_texture_cache.c').read_text()
    load = function(source,'texture_cache_start_loading_bitmap')
    assert '_android_cache_test_rejected' in load
    assert load.index('lruv_block_new(') < load.index('_android_cache_test_allocation_failed')
    assert load.count('lruv_block_new(') == 2
    get = function(source,'_texture_cache_bitmap_get_hardware_format')
    assert get.index('return hardware_format;') < get.index('_android_cache_test_fallback')
    assert get.index('if (block && !hardware_format)') < get.index('_android_cache_test_fallback')
    idle = function(source,'texture_cache_idle')
    assert idle.index('android_cache_test_tick') < idle.index('lruv_idle')
    for name in ('texture_cache_open','texture_cache_close'):
        assert 'android_cache_test_reset' in function(source,name)
    stripped = re.sub(r'#ifdef HALO_ANDROID\n.*?#endif', '', source, flags=re.S)
    assert 'android_cache_test_' not in stripped
