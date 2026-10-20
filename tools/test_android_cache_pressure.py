"""Reproduce current-frame exhaustion using the game's real allocator."""
from pathlib import Path
import re
import pytest
from harness import function
from test_android_texture_compact import compact_program
from test_android_terrain import run_c

ROOT = Path(__file__).resolve().parents[1]


def test_real_allocator_cannot_compact_away_current_frame_pressure(tmp_path):
    # 36 MiB, with the 1,398,144-byte terrain request from the player's log.
    # This is a deterministic allocator reproduction, not a gameplay trace.
    code = compact_program() + r'''
int main(void) {
 struct android_texture_compact_result r;
 long i;
 setup();
 for(i=0;i<8;i++) { blocks.items[i].last_used_tick=1;lruv_block_delete(&cache,i); }
 cache.page_count=2304;
 assert(lruv_block_new(&cache,2250*16384)!=NONE);
 assert(lruv_block_new(&cache,1398144)==NONE);
 android_texture_compact(&cache,1398144,texture_cache_move_android_block,&r);
 assert(r.status==0 && r.moved_pages==0 && r.evicted_blocks==0);
 /* A second attempt cannot make room while the same frame owns the pages. */
 assert(lruv_block_new(&cache,1398144)==NONE);
 return 0;
}
'''
    run_c(tmp_path, code)


def storage_program():
    gpu = (ROOT/'port/linux/src/xbox_textures.c').read_text()
    test = (ROOT/'tools/test_android_texture_cache.py').read_text()
    code = '#define HALO_ANDROID 1\n' + test.split("code = defines + r'''", 1)[1].split("'''", 1)[0]
    code = code.replace('static const void *android_ce_texture_pixels(const void *r,unsigned long *s) { *s=0;return NULL; }', '')
    code = code.replace('static BOOL platform_is_contiguous(void *p) { return TRUE; }',
                        'static BOOL platform_is_contiguous(void *p) { return FALSE; }')
    code += r'''
struct bitmap_data { void *base_address; };
static unsigned long reads, unbinds, channel_forgets;
static int fail_allocation;
static void *storage_calloc(size_t n,size_t s) {
 if(fail_allocation && --fail_allocation==0)return NULL;
 return calloc(n,s);
}
static void android_texture_test_log(const char *f,...) {}
void android_texture_test_pressure(unsigned long a,unsigned long b,unsigned long c,unsigned long d,unsigned long e) {}
static void halo_custom_edition_texels_channels(const void *p,unsigned char order) { channel_forgets++; }
void android_ce_texture_unbind(const void *r) { unbinds++; }
'''
    code += (ROOT/'port/linux/game/android_ce_texture_cache.h').read_text()
    storage = (ROOT/'port/linux/game/android_ce_texture_cache.c').read_text()
    # cseries redirects free, but not calloc. Exercise the production source's
    # allocator pairing, including unload, instead of hiding it in this fixture.
    code += '#define free(p) abort()\n#define calloc storage_calloc\n'
    code += re.sub(r'^#include .*$', '', storage, flags=re.M)
    code += '#undef calloc\n'
    code += gpu[gpu.index('struct texture_entry\n'):gpu.index('/* (every bit of the three mixed')]
    for name in ('bucket_index', 'palette_hash', 'android_ce_texture_gpu_forget', 'texture_entry_pixels_valid'):
        code += function(gpu, name) + '\n'
    code += r'''
static GLuint texture_entry_result(struct texture_entry *e,GLenum *t,struct xgpu_texture_description *d) {
 *t=e->target;*d=e->description;return e->texture;
}
static void read_pixels(struct bitmap_data *b,void *h) {
 unsigned long *header=h;reads++;header[3]=0;header[4]=0;
 ((unsigned char *)b->base_address)[0]=42;
}
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
'''
    return code + function(gpu, 'xgpu_texture_get')


@pytest.mark.parametrize('scenario', ['working_set', 'eviction', 'capacity', 'reload', 'bounds', 'palette', 'mcc', 'oom', 'resident'])
def test_stable_ce_storage_and_real_gpu_lookup(tmp_path, scenario):
    cases = {
        'working_set': r'''
/* 80 MiB of same-frame demand exceeds the old fixed 36 MiB arena.
 * Repeated frames must not reload pixels, relocate them, or re-upload them. */
for(i=0;i<80;i++) {
 headers[i]=android_ce_texture_get(&bitmaps[i],1024*1024,1,read_pixels);CHECK(headers[i]);
 CHECK(xgpu_texture_get(headers[i],NULL,&target,&description)!=0);
}
CHECK(reads==80 && uploads==80 && cache_bytes==80UL*1024*1024);
for(frame=0;frame<120;frame++) {
 android_ce_texture_idle();serial++;generation++;
 for(i=0;i<80;i++) {
  CHECK(android_ce_texture_get(&bitmaps[i],1024*1024,1,read_pixels)==headers[i]);
  xgpu_texture_get(headers[i],NULL,&target,&description);
 }
}
CHECK(reads==80 && uploads==80 && !cache_evictions && !cache_failed);
android_ce_texture_reset();CHECK(cache_bytes==0 && cache_count==0 && unbinds==80);
for(i=0;i<80;i++)CHECK(bitmaps[i].base_address==NULL);
for(i=0;i<TEXTURE_BUCKET_COUNT;i++)CHECK(texture_buckets[i]==NULL);''',
        'eviction': r'''
headers[0]=android_ce_texture_get(&bitmaps[0],80UL*1024*1024,1,read_pixels);CHECK(headers[0]);
xgpu_texture_get(headers[0],NULL,&target,&description);
android_ce_texture_idle();
headers[1]=android_ce_texture_get(&bitmaps[1],60UL*1024*1024,1,read_pixels);CHECK(headers[1]);
CHECK(!bitmaps[0].base_address && cache_evictions==1 && unbinds==1 && cache_bytes==60UL*1024*1024);
CHECK(android_ce_texture_get(&bitmaps[0],1024,0,read_pixels)==NULL);
xgpu_texture_get(headers[1],NULL,&target,&description);CHECK(uploads==2);
android_ce_texture_reset();''',
        'capacity': r'''
CHECK(android_ce_texture_get(&bitmaps[0],80UL*1024*1024,1,read_pixels));
CHECK(!android_ce_texture_get(&bitmaps[1],60UL*1024*1024,1,read_pixels));
CHECK(cache_failed==1 && cache_count==1 && !unbinds);
CHECK(!android_ce_texture_get(&bitmaps[1],~0UL,1,read_pixels));
CHECK(!android_ce_texture_get(&bitmaps[1],0,1,read_pixels));
android_ce_texture_reset();''',
        'reload': r'''
for(i=0;i<20;i++) {
 headers[0]=android_ce_texture_get(&bitmaps[0],1024,1,read_pixels);CHECK(headers[0]);
 xgpu_texture_get(headers[0],NULL,&target,&description);CHECK(uploads==(unsigned long)i+1);
 android_ce_texture_delete(&bitmaps[0]);
 CHECK(!bitmaps[0].base_address && cache_bytes==0);
 CHECK(!android_ce_texture_pixels(headers[0],&bytes) && bytes==0);
}
CHECK(unbinds==20 && channel_forgets==20);''',
        'bounds': r'''
headers[0]=android_ce_texture_get(&bitmaps[0],512,1,read_pixels);CHECK(headers[0]);
xgpu_texture_get(headers[0],NULL,&target,&description);CHECK(uploads==0);
/* A matching Data word without the registered header grants no heap access. */
memcpy(fake,headers[0],sizeof(fake));
xgpu_texture_get(fake,NULL,&target,&description);CHECK(uploads==0);
android_ce_texture_reset();''',
        'palette': r'''
headers[0]=android_ce_texture_get(&bitmaps[0],1024,1,read_pixels);CHECK(headers[0]);
((DWORD *)headers[0])[3]=11;
xgpu_texture_get(headers[0],palette,&target,&description);CHECK(uploads==1);
palette[5]=42;xgpu_texture_get(headers[0],palette,&target,&description);CHECK(uploads==2);
palette[5]=0;xgpu_texture_get(headers[0],palette,&target,&description);CHECK(uploads==2);
android_ce_texture_reset();
for(i=0;i<TEXTURE_BUCKET_COUNT;i++)CHECK(texture_buckets[i]==NULL);''',
        'mcc': r'''
mcc=TRUE;xgpu_texture_get(fake,NULL,&target,&description);
CHECK(!uploads && !reads && !cache_count);''',
        'oom': r'''
for(i=1;i<=2;i++) {
 fail_allocation=i;
 CHECK(!android_ce_texture_get(&bitmaps[0],1024,1,read_pixels));
 CHECK(!bitmaps[0].base_address && !cache_bytes && !cache_count && !reads);
}
fail_allocation=0;
CHECK(android_ce_texture_get(&bitmaps[0],1024,1,read_pixels));
android_ce_texture_reset();''',
        'resident': r'''
CHECK(!android_ce_texture_resident(&bitmaps[0]));
headers[0]=android_ce_texture_get(&bitmaps[0],80UL*1024*1024,1,read_pixels);
CHECK(headers[0] && android_ce_texture_resident(&bitmaps[0]));
android_ce_texture_idle();
/* HUD classification must not pin every registered bitmap for this frame. */
CHECK(android_ce_texture_resident(&bitmaps[0]));
CHECK(android_ce_texture_get(&bitmaps[1],60UL*1024*1024,1,read_pixels));
CHECK(!android_ce_texture_resident(&bitmaps[0]));
cache_frame=~0UL;android_ce_texture_idle();CHECK(cache_frame==1);
CHECK(android_ce_texture_get(&bitmaps[0],80UL*1024*1024,1,read_pixels));
android_ce_texture_reset();''',
    }
    code = storage_program() + r'''
int main(void) {
 struct bitmap_data bitmaps[80]={{0}};void *headers[80];
 DWORD fake[5]={0};D3DCOLOR palette[256]={0};unsigned long bytes;
 GLenum target;struct xgpu_texture_description description;int i,frame;
''' + cases[scenario] + '\nreturn 0;}\n'
    run_c(tmp_path, code)


def test_android_ce_scope_and_cleanup():
    source = (ROOT/'source/cache/xbox_texture_cache.c').read_text()
    get = function(source, '_texture_cache_bitmap_get_hardware_format')
    assert get.index('mcc_texture_cache_get') < get.index('android_ce_texture_get')
    assert 'custom_edition_cache_tags_loaded() && !mcc_cache_tags_loaded()' in get
    assert 'TEST_FLAG(bitmap->flags, _bitmap_cached_bit)' in get
    assert 'texture_cache_bitmap_valid(bitmap)' in get
    stripped = re.sub(r'#ifdef HALO_ANDROID\n.*?#endif', '', source, flags=re.S)
    assert 'android_ce_texture_' not in stripped
    assert 'android_ce_texture_delete(bitmap)' in function(source, 'texture_cache_bitmap_delete')
    assert 'android_ce_texture_reset()' in function(source, 'texture_cache_flush')


@pytest.mark.parametrize('android', [False, True])
def test_hud_residency_uses_the_correct_owner(tmp_path, android):
    source = (ROOT/'port/linux/game/hud_hires_tags.c').read_text()
    code = ('#define HALO_ANDROID 1\n' if android else '') + r'''
#define TRUE 1
#define NONE -1
typedef int boolean;
struct bitmap_data { long cache_block_index; };
static int resident;
static int android_ce_texture_resident(const struct bitmap_data *b) { return resident; }
''' + function(source, 'hud_hires_bitmap_resident') + r'''
int main(void) {
 struct bitmap_data b={NONE};
 if(hud_hires_bitmap_resident(&b))return 1;
 b.cache_block_index=3;if(!hud_hires_bitmap_resident(&b))return 2;
 b.cache_block_index=NONE;resident=1;
'''
    code += f'if(hud_hires_bitmap_resident(&b)!={int(android)})return 3;\nreturn 0;}}\n'
    run_c(tmp_path, code)
