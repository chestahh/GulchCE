"""Exercise real LRU allocation, compaction, pixel moves and retained headers."""
from pathlib import Path
import re
import pytest
from harness import function, structure
from test_android_terrain import run_c

ROOT = Path(__file__).resolve().parents[1]


def compact_program():
    lru = (ROOT/'source/memory/lruv_cache.c').read_text()
    header = (ROOT/'source/memory/lruv_cache.h').read_text()
    texture = (ROOT/'source/cache/xbox_texture_cache.c').read_text()
    gpu = (ROOT/'port/linux/src/xbox_textures.c').read_text()
    compact = (ROOT/'port/linux/game/android_texture_compact.c').read_text()
    code = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define NONE -1
#define TRUE 1
#define FALSE 0
#define HALO_PORT_TEXTURE_CACHE_PAGE_COUNT 2304
#define MAXIMUM_LRUV_CACHE_HOLES 256
#define NEXT_HOLE_INDEX(i) ((i)==255?0:(i)+1)
#define match_assert(file,line,expr) assert(expr)
#define lruv_cache_block_get(cache,index) datum_get((cache)->blocks,index)
typedef int boolean;
typedef unsigned char byte;
typedef void (*lruv_delete_block_proc)(long);
typedef boolean (*lruv_locked_block_proc)(long);
'''
    for name in ('lruv_cache','lruv_cache_block'):
        code += structure(header, name)
    code += structure(lru, 'lruv_cache_hole')
    code += (ROOT/'port/linux/game/android_texture_compact.h').read_text()
    code += r'''
struct data_array { int actual_count,maximum_count; struct lruv_cache_block items[16]; int active[16]; };
struct data_iterator { struct data_array *array; long datum_index; };
static void *datum_get(struct data_array *a,long i) { assert(i>=0 && i<16 && a->active[i]);return &a->items[i]; }
static long datum_new(struct data_array *a) { int i;for(i=0;i<16;i++) if(!a->active[i]) {a->active[i]=1;a->actual_count++;return i;}return NONE; }
static void datum_delete(struct data_array *a,long i) {assert(a->active[i]);a->active[i]=0;a->actual_count--;}
static void data_iterator_new(struct data_iterator *i,struct data_array *a) {i->array=a;i->datum_index=-1;}
static void *data_iterator_next(struct data_iterator *i) {while(++i->datum_index<16) if(i->array->active[i->datum_index]) return datum_get(i->array,i->datum_index);return NULL;}
static void lruv_cache_verify(struct lruv_cache *c,boolean full) {
 long i=c->first_block_index,prev=-1,end=0,count=0;struct lruv_cache_block *b;
 while(i!=-1) {b=datum_get(c->blocks,i);assert(b->previous_block_index==prev && b->first_page_index>=end);
 end=b->first_page_index+b->page_count;assert(end<=c->page_count);prev=i;i=b->next_block_index;assert(++count<=16);}
 assert(prev==c->last_block_index && count==c->blocks->actual_count);
}
static struct data_array blocks;
static struct lruv_cache cache;
static unsigned char pixels[20*16384];
static int busy=-1,deleted[16],write_calls,forget_calls;
static unsigned long write_generation;
static boolean locked(long i) {return i==busy;}
static void deleted_block(long i) {assert(blocks.items[i].last_used_tick!=cache.tick);deleted[i]++;}
struct bitmap_data {byte *base_address;};
typedef struct {unsigned long Common,Data,Lock,Format,Size;} D3DBaseTexture;
struct xbox_texture_cache_texture {struct bitmap_data *bitmap;D3DBaseTexture hardware_format;};
static struct bitmap_data bitmaps[16];
static struct xbox_texture_cache_texture entries[16];
static struct {void *textures;byte *base_address;} xbox_texture_cache_globals;
static void *texture_datum_get(void *a,long i) {return &entries[i];}
static void IDirect3DBaseTexture8_Register(D3DBaseTexture *t,void *p) {t->Data+=(unsigned long)p;}
#define _custom_edition_channels_xbox 0
#define NUMBER_OF_CUSTOM_EDITION_CHANNEL_ORDERS 3
#define platform_log(...) ((void)0)
'''
    code += structure(gpu,'custom_edition_texels')
    code += r'''
static struct custom_edition_texels *custom_edition_texels;
static unsigned long custom_edition_texel_count,custom_edition_texel_capacity;
static void memory_watch_prepare_write(void *p,unsigned long s) {assert((byte *)p>=pixels && (byte *)p+s<=pixels+sizeof(pixels));write_calls++;}
static void memory_watch_forget(void *p,unsigned long s) {assert(write_calls>forget_calls);forget_calls++;write_generation++;}
'''
    for name in ('custom_edition_texels_order','halo_custom_edition_texels_channels','halo_custom_edition_texels_forget','android_texture_move_texels'):
        code += function(gpu,name)+'\n'
    code += '#define datum_get texture_datum_get\n'+function(texture,'texture_cache_move_android_block')+'\n#undef datum_get\n'
    for name in ('lruv_cache_bytes_to_pages','lruv_block_delete','lruv_block_new'):
        code += function(lru,name)+'\n'
    code += '#undef index\n'+function(compact,'android_texture_compact')
    code += r'''
static void setup(void) {
 int i;long id;memset(&cache,0,sizeof(cache));memset(&blocks,0,sizeof(blocks));memset(deleted,0,sizeof(deleted));
 halo_custom_edition_texels_forget();busy=-1;write_calls=forget_calls=0;write_generation=0;
 cache.blocks=&blocks;blocks.maximum_count=16;cache.page_count=16;cache.page_size_bits=14;
 cache.tick=10;cache.first_block_index=cache.last_block_index=-1;cache.locked_block_proc=locked;cache.delete_block_proc=deleted_block;
 xbox_texture_cache_globals.base_address=pixels;
 memset(pixels,0xcd,sizeof(pixels));
 for(i=0;i<8;i++) {
  id=lruv_block_new(&cache,2*16384);assert(id==i);
  bitmaps[i].base_address=pixels+blocks.items[i].first_page_index*16384;
  memset(bitmaps[i].base_address,i+1,2*16384);
  entries[i].bitmap=&bitmaps[i];entries[i].hardware_format.Data=(unsigned long)bitmaps[i].base_address;
  entries[i].hardware_format.Common=17;entries[i].hardware_format.Format=123;entries[i].hardware_format.Size=456;
  halo_custom_edition_texels_channels(bitmaps[i].base_address,(i%2)+1);
 }
 for(i=0;i<8;i++) if(!(i%2)) blocks.items[i].last_used_tick=i+1;
}
static void verify_pixels(void) {
 int i,j;struct lruv_cache_block *b;
 lruv_cache_verify(&cache,1);
 for(i=0;i<8;i++) if(blocks.active[i] && !deleted[i]) {
  b=&blocks.items[i];assert(bitmaps[i].base_address==pixels+b->first_page_index*16384);
  assert(entries[i].hardware_format.Data==(unsigned long)bitmaps[i].base_address);
  assert(entries[i].hardware_format.Common==17 && entries[i].hardware_format.Format==123 && entries[i].hardware_format.Size==456);
  assert(custom_edition_texels_order((unsigned long)bitmaps[i].base_address)==(i%2)+1);
  for(j=0;j<b->page_count*16384;j++) assert(bitmaps[i].base_address[j]==i+1);
 }
 assert(write_calls==forget_calls);
}
'''
    return code


@pytest.mark.parametrize('scenario', ['recover','busy','capacity','holes','repeat','overlap','invalid','reduced'])
def test_compaction_with_real_allocator(tmp_path, scenario):
    cases = {
        'recover': '''assert(lruv_block_new(&cache,6*16384)==NONE);
android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);
assert(r.status==1 && r.evicted_blocks==3 && r.moved_pages==10);
assert(deleted[0] && deleted[2] && deleted[4] && !deleted[6]);verify_pixels();
assert(entries[1].hardware_format.Data==(unsigned long)pixels);
assert(lruv_block_new(&cache,6*16384)!=NONE);''',
        'busy': '''busy=7;android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);
assert(r.status==2 && !r.evicted_blocks && !r.moved_pages && !write_calls);verify_pixels();''',
        'capacity': '''android_texture_compact(&cache,9*16384,texture_cache_move_android_block,&r);
assert(r.status==0 && !r.evicted_blocks && !write_calls);verify_pixels();''',
        'holes': '''lruv_block_delete(&cache,0);lruv_block_delete(&cache,2);lruv_block_delete(&cache,4);
assert(lruv_block_new(&cache,6*16384)==NONE);
android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);
assert(r.status==1 && !r.evicted_blocks);verify_pixels();assert(lruv_block_new(&cache,6*16384)!=NONE);''',
        'repeat': '''android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);verify_pixels();
android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);
assert(r.status==1 && !r.moved_pages && !r.evicted_blocks);verify_pixels();
setup();android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);verify_pixels();''',
        'overlap': '''/* Exercise overlapping memmove plus channel annotation replacement. */
memset(pixels,9,3*16384);halo_custom_edition_texels_channels(pixels+16384,2);
android_texture_move_texels(pixels,pixels+16384,3*16384);
assert(pixels[0]==9 && pixels[2*16384]==2 && custom_edition_texels_order((unsigned long)pixels)==2);
assert(custom_edition_texels_order((unsigned long)(pixels+16384))==0 && write_generation==1);''',
        'invalid': '''android_texture_compact(&cache,0,texture_cache_move_android_block,&r);
assert(r.status==0 && !write_calls);cache.page_size_bits=12;
android_texture_compact(&cache,16384,texture_cache_move_android_block,&r);assert(!r.status && !write_calls);''',
        'reduced': '''/* A temporarily reduced cache must not consume the stolen tail. */
blocks.items[7].last_used_tick=1;lruv_block_delete(&cache,7);cache.page_count=14;
android_texture_compact(&cache,6*16384,texture_cache_move_android_block,&r);assert(r.status==1);verify_pixels();
assert(pixels[16*16384]==0xcd);assert(lruv_block_new(&cache,6*16384)!=NONE);lruv_cache_verify(&cache,1);''',
    }
    code=compact_program()+'int main(void) {struct android_texture_compact_result r;setup();\n'+cases[scenario]+'\nreturn 0;}\n'
    run_c(tmp_path,code)


def test_platform_and_family_scope():
    source=(ROOT/'source/cache/xbox_texture_cache.c').read_text()
    load=function(source,'texture_cache_start_loading_bitmap')
    assert 'cache_block_index == NONE && custom_edition_cache_tags_loaded() && !mcc_cache_tags_loaded()' in load
    stripped=re.sub(r'#ifdef HALO_ANDROID\n.*?#endif','',source,flags=re.S)
    assert 'android_texture_compact' not in stripped and 'android_texture_move_texels' not in stripped
    get=function(source,'_texture_cache_bitmap_get_hardware_format')
    assert get.index('mcc_texture_cache_get') < get.index('texture_cache_start_loading_bitmap')
    # The platform retains stable header pointers; relocation cannot rely on a rebind.
    state=(ROOT/'port/linux/src/d3d8_gl.c').read_text()
    assert 'device.textures[stage] = texture;' in function(state,'D3DDevice_SetTexture')
