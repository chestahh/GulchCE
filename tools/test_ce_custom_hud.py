"""CE mixed-use HUD sheets choose channels per draw, not per bitmap."""
from pathlib import Path
from harness import function
from test_hud_button_style import run_c

ROOT = Path(__file__).resolve().parents[1]


def test_warning_layout_and_map_family_boundaries(tmp_path):
    source = (ROOT / 'port/linux/game/custom_edition_hud.h').read_text()
    body = '\n'.join(function(source, n) for n in [
        'custom_edition_hud_warning', 'custom_edition_hud_warning_placement'])
    run_c(tmp_path, r'''
#include <assert.h>
typedef int boolean;
#define SHORT_MIN -32768
#define SHORT_MAX 32767
#define PIN(v,a,b) ((v)<(a)?(a):(v)>(b)?(b):(v))
#define _custom_edition_behaviour_old_widescreen_fix 0
static int ce=1,legacy=1;
static int custom_edition_cache_tags_loaded(void) {return ce;}
static int custom_edition_cache_relies_on(int flag) {return ce&&legacy;}
struct hud_placement_definition {struct {short x,y;}offset;};
PRODUCTION
int main(void) {
    struct hud_placement_definition p={{155,-210}};
    int state;
    assert(!custom_edition_hud_warning(0)); /* Aim still follows FOV. */
    assert(!custom_edition_hud_warning(1)); /* Scope keeps its own path. */
    for(state=3;state<=15;state++) assert(custom_edition_hud_warning(state));
    custom_edition_hud_warning_placement(3,624,464,&p);
    assert(p.offset.x==155 && p.offset.y==-210);
    custom_edition_hud_warning_placement(7,724,408,&p);
    assert(p.offset.x==205 && p.offset.y==-182);
    p.offset.x=0;custom_edition_hud_warning_placement(3,724,408,&p);assert(!p.offset.x);
    p.offset.x=-155;custom_edition_hud_warning_placement(3,724,408,&p);assert(p.offset.x==-205);
    p.offset.x=155;legacy=0;custom_edition_hud_warning_placement(3,724,408,&p);assert(p.offset.x==155);
    ce=0;legacy=1;custom_edition_hud_warning_placement(3,724,408,&p);
    assert(p.offset.x==155 && !custom_edition_hud_warning(3));
    return 0;
}
'''.replace('PRODUCTION', body))


def test_shared_meter_channels_do_not_leak_or_convert_redraws(tmp_path):
    source = (ROOT / 'port/linux/src/xbox_textures.c').read_text()
    body = '\n'.join(function(source, n) for n in ['halo_custom_edition_hud_meter', 'texture_entry_result', 'halo_custom_edition_texels_forget'])
    run_c(tmp_path, r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int GLuint,GLenum;
#define TRUE 1
#define GL_TEXTURE_2D 2
#define _texel_p8 1
#define _custom_edition_channels_xbox 0
static int custom_edition_hud_meter_active,order,enabled;
static void *custom_edition_texels;
static unsigned long custom_edition_texel_count,custom_edition_texel_capacity;
struct xgpu_texture_description {unsigned long levels,width,height;int compressed,format,hires,hires_coverage,hires_point_threshold,ce_meter_channels;};
struct texture_entry {GLuint target,texture;unsigned long data,address,size,hud_upscale_levels;int override,hud_upscale_checked;GLuint hud_upscale;struct xgpu_texture_description description;};
#define text_hires_atlas_texture(a) 0
#define menu_art_texture(a,b) 0
#define hud_hires_placeholder_texture(a,b) 0
#define hud_hires_override_texture(a,b) 99
#define hud_hires_override_coverage(a) 1
#define hud_hires_override_point_threshold(a) 1
#define config_boolean(a) enabled
#define hud_hires_upscale_at(a,b,c) 0
static struct {int kind;} info;
#define format_information(f) info
#define custom_edition_texels_order(a) order
#define platform_is_contiguous(a) 1
#define decode_level(a,b,c,d,e) 0
#define hud_hires_upscale_texture(a,b,c,d,e) 0
PRODUCTION
int main(void) {
    struct texture_entry entry={0};struct xgpu_texture_description result;GLenum target;
    entry.override=-1;entry.texture=7;
    halo_custom_edition_hud_meter(1);
    assert(texture_entry_result(&entry,&target,&result)==7 && result.ce_meter_channels);
    halo_custom_edition_hud_meter(0);
    assert(texture_entry_result(&entry,&target,&result)==7 && !result.ce_meter_channels);
    halo_custom_edition_hud_meter(1);order=2; /* Already converted meter. */
    texture_entry_result(&entry,&target,&result);assert(!result.ce_meter_channels);
    order=3; /* Explicit Xbox-order custom meter artwork. */
    texture_entry_result(&entry,&target,&result);assert(!result.ce_meter_channels);
    order=0;entry.override=0; /* Stock CE high-resolution meter has Xbox channels. */
    assert(texture_entry_result(&entry,&target,&result)==99 && !result.ce_meter_channels);
    entry.override=-1;enabled=1;
    texture_entry_result(&entry,&target,&result);assert(result.ce_meter_channels);
    halo_custom_edition_texels_forget();
    texture_entry_result(&entry,&target,&result);assert(!result.ce_meter_channels);
    return 0;
}
'''.replace('PRODUCTION', body))


def test_meter_scope_and_shader_are_ce_only():
    source = function((ROOT / 'source/interface/hud_draw.c').read_text(), 'hud_draw_bitmap_internal')
    begin = source.index('halo_custom_edition_hud_meter(meter_parameters && custom_edition_cache_tags_loaded())')
    draw = source.index('rasterizer_psuedo_dynamic_screen_quad_draw(')
    end = source.index('halo_custom_edition_hud_meter(FALSE)')
    assert begin < draw < end
    shader = (ROOT / 'port/linux/src/nv2a_psh.c').read_text()
    assert 'if (stage == 0 && key->ce_meter_channels)' in shader
    assert 't0 = t0.aaar;' in shader
    texture = function((ROOT / 'port/linux/src/xbox_textures.c').read_text(), 'xgpu_texture_get')
    assert texture.index('xgpu_mcc_texture_get(') < texture.index('texture_entry_result(')


def test_authored_xbox_ammo_sheet_keeps_identity_through_static_use(tmp_path):
    source = (ROOT / 'port/linux/game/custom_edition_bitmaps.c').read_text()
    body = '\n'.join(function(source, n) for n in [
        'reordered_bitmap_index', 'reordered_bitmap_mark', 'meter_bitmaps_mark',
        'custom_edition_bitmaps_dispose'])
    run_c(tmp_path, r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
typedef unsigned char byte;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define _error_silent 0
#define error(a,b,c) ((void)0)
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(i) ((i)&65535)
#define _custom_edition_channels_hud_meter 2
#define _custom_edition_channels_hud_meter_xbox 3
#define _custom_edition_behaviour_xbox_ammo_meter_channels 12
struct reordered_bitmap {long handle;short bitmap_index;byte channel_order;int drawn_otherwise;};
struct custom_edition_bitmaps_globals {struct reordered_bitmap *reordered_bitmaps;
    unsigned long behaviours;long reordered_bitmap_count,reordered_bitmap_capacity;};
static struct custom_edition_bitmaps_globals custom_edition_bitmaps_globals;
struct tag_view {int unused;};
struct meter_hud_element_definition {struct {long index;}meter_bitmap;short sequence_index;};
static const char *name;
static const char *view_tag_name(const struct tag_view *v,long i) {return name;}
static int forgotten;
static void halo_custom_edition_texels_forget(void) {forgotten++;}
static boolean reordered_bitmaps_mark(const struct tag_view *,long,short,short);
PRODUCTION
static boolean reordered_bitmaps_mark(const struct tag_view *v,long h,short s,short o) {
    return reordered_bitmap_mark(h,s,o);
}
int main(void) {
    struct tag_view view={0};struct meter_hud_element_definition meter={{100},1};
    struct custom_edition_bitmaps_globals *g=&custom_edition_bitmaps_globals;
    name="bourrin\\hud\\v3\\bitmaps\\all icons wips";
    assert(meter_bitmaps_mark(&view,&meter));
    assert(g->reordered_bitmaps[0].channel_order==2); /* Legacy CE. */
    g->reordered_bitmap_count=0;g->behaviours=1UL<<12;
    assert(meter_bitmaps_mark(&view,&meter));
    assert(g->reordered_bitmaps[0].channel_order==3);
    reordered_bitmap_mark(100,1,NONE); /* Same sheet's static weapon icon. */
    assert(!g->reordered_bitmaps[0].drawn_otherwise);
    name="other custom meter";meter.meter_bitmap.index=101;
    assert(meter_bitmaps_mark(&view,&meter));
    assert(g->reordered_bitmaps[1].channel_order==2);
    reordered_bitmap_mark(101,1,NONE);
    assert(g->reordered_bitmaps[1].drawn_otherwise);
    custom_edition_bitmaps_dispose();
    assert(!g->behaviours && !g->reordered_bitmap_count && !g->reordered_bitmaps && forgotten==1);
    name="bourrin\\hud\\v3\\bitmaps\\all icons wips";
    assert(meter_bitmaps_mark(&view,&meter)); /* Next map has no exception. */
    assert(g->reordered_bitmaps[0].channel_order==2);
    custom_edition_bitmaps_dispose();
    return 0;
}
'''.replace('PRODUCTION', body))
