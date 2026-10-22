"""CE HUD registration and pixel identity regression checks."""
from pathlib import Path
import copy
import json
import struct
import zlib

import pytest
from harness import function
from test_hud_button_style import run_c
from ce_hud_signatures import fingerprint, region
from ce_hud_assets import build_asset
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def test_stock_fallback_checks_pixels_and_bounds(tmp_path):
    body = function((ROOT / 'port/linux/src/hud_hires.c').read_text(), 'hud_hires_ce_stock_matches')
    run_c(tmp_path, r'''
#include <assert.h>
typedef unsigned char Bytef;
typedef unsigned int uInt;
static struct hud_hires_ce_stock {char const *tag;int bitmap;unsigned int width,height,size,crc;}
    hud_hires_ce_stock[]={{"stock",0,4,4,16,7}};
static unsigned int hud_hires_ce_stock_count=1;
static unsigned long crc32(unsigned long c,Bytef const *p,uInt n) {return p[0];}
PRODUCTION
int main(void) {
    unsigned char pixels[16]={7};
    unsigned long address=(unsigned long)pixels;
    assert(hud_hires_ce_stock_matches(0,address,4,4,16));
    pixels[0]=8;
    assert(!hud_hires_ce_stock_matches(0,address,4,4,16));
    assert(!hud_hires_ce_stock_matches(-1,address,4,4,16));
    assert(!hud_hires_ce_stock_matches(1,address,4,4,16));
    assert(!hud_hires_ce_stock_matches(0,0,4,4,16));
    pixels[0]=7;
    assert(!hud_hires_ce_stock_matches(0,address,8,4,16));
    assert(!hud_hires_ce_stock_matches(0,address,4,8,16));
    assert(!hud_hires_ce_stock_matches(0,address,4,4,8));
    return 0;
}
'''.replace('PRODUCTION', body))


def test_ce_registers_before_mods_and_mcc_stays_separate():
    source = function((ROOT / 'source/cache/cache_files.c').read_text(), 'scenario_tags_load')
    ce = source[source.index('if (custom_edition_level_name(scenario_name))'):]
    ce = ce[:ce.index('if (cache_file_open')]
    assert ce.index('global_tag_instances =') < ce.index('hud_hires_tags_loaded(')
    assert ce.index('hud_hires_tags_loaded(') < ce.index('mods_tags_loaded(')
    mcc = source[source.index('if (mcc_level_name(scenario_name))'):source.index('if (custom_edition_level_name(scenario_name))')]
    assert 'hud_hires_tags_loaded(' not in mcc
    unload = function((ROOT / 'source/cache/cache_files.c').read_text(), 'scenario_tags_unload')
    assert 'hud_hires_tags_unloaded();' in unload


def test_override_identity_and_setting(tmp_path):
    body = function((ROOT / 'port/linux/src/hud_hires.c').read_text(), 'hud_hires_override_find')
    run_c(tmp_path, r'''
#include <assert.h>
#include <string.h>
typedef unsigned char Bytef;
typedef unsigned int uInt;
static struct hud_hires_embedded { int title; unsigned long ce_width,ce_height,ce_size,ce_crc,crc;
    char *tag; int bitmap; } hud_hires_embedded[] = {{0,4,4,16,7,9,"test",0}};
static struct {int other_pixels_logged;} textures[1];
static unsigned long generation;
static int enabled=1, ce=1, active=1;
static unsigned long config_changes(void) {return generation;}
static int config_boolean(char const *s) {return enabled;}
static long hud_hires_asset_at(unsigned long a,long w,long h) {return active ? 0 : -1;}
static long hud_hires_asset_count(void) {return 1;}
static int hud_hires_asset_stock_custom_edition(unsigned long a,long w,long h,long i) {return ce;}
/* Control the CRC provider independently of the production decision. */
static unsigned long crc32(unsigned long c,Bytef const *p,uInt n) {return p[0];}
#define platform_log(...) ((void)0)
PRODUCTION
int main(void) {
    unsigned char pixels[16]={7};
    unsigned long address=(unsigned long)pixels;
    assert(hud_hires_override_find(address,4,4,16)==0);
    enabled=0; generation++;
    assert(hud_hires_override_find(address,4,4,16)==-1);
    enabled=1; generation++;
    assert(hud_hires_override_find(address,4,4,16)==0);
    pixels[0]=8;
    assert(hud_hires_override_find(address,4,4,16)==-1);
    pixels[0]=7;
    assert(hud_hires_override_find(address,8,4,16)==-1);
    assert(hud_hires_override_find(address,4,8,16)==-1);
    assert(hud_hires_override_find(address,4,4,8)==-1);
    active=0; /* unload / MCC without registration */
    assert(hud_hires_override_find(address,4,4,16)==-1);
    active=1; ce=0; pixels[0]=9; /* existing Xbox CRC path */
    assert(hud_hires_override_find(address,4,4,16)==0);
    pixels[0]=7;
    assert(hud_hires_override_find(address,4,4,16)==-1);
    return 0;
}
'''.replace('PRODUCTION', body))


def test_fingerprint_hardware_order_and_bounds():
    item = bytearray(0x6c + 48)
    struct.pack_into('<II', item, 0x60, 1, 0x6c)
    struct.pack_into('<6H', item, 0x70, 4, 2, 1, 0, 0, 1)
    struct.pack_into('<II', item, 0x84, 0, 8)
    signature = fingerprint(bytes(range(8)), item, 0)
    assert signature == dict(width=4, height=2, size=8,
                             crc=zlib.crc32(bytes([0, 1, 4, 5, 2, 3, 6, 7])))
    with pytest.raises(ValueError):
        fingerprint(bytes(7), item, 0)
    with pytest.raises(ValueError):
        fingerprint(bytes(8), item, 1)
    with pytest.raises(ValueError):
        region(bytes(8), -1, 1)


def test_registration_preserves_custom_sheets(tmp_path):
    source = (ROOT / 'port/linux/game/hud_hires_tags.c').read_text()
    globals_ = source[source.index('static struct'):source.index('static void hud_buttons_forget')]
    body = function(source, 'hud_hires_tags_loaded')
    run_c(tmp_path, r'''
#include <assert.h>
#include <string.h>
#define MAXIMUM_HIRES_BITMAPS 128
#define MAXIMUM_HIRES_SPRITE_BITMAPS 8
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define NONE -1
#define BITMAP_GROUP_TAG 1
#define _scenario_type_main_menu 2
#define csmemset memset
#define platform_log(...) ((void)0)
typedef int boolean;
struct bitmap_data {short width,height;};
static struct bitmap_data bitmap={128,32};
static struct {int type;} scenario;
static int ce=1,resource=1,compatible=1,ce_only=0;
static long hud_hires_asset_count(void) {return 1;}
static char const *hud_hires_asset_tag(long a) {return "stock";}
static long hud_hires_asset_bitmap(long a) {return 0;}
static long tag_loaded(long group,char const *name) {return 1;}
static struct bitmap_data *bitmap_group_try_and_get_bitmap(long group,short index) {return &bitmap;}
static int custom_edition_cache_tags_loaded(void) {return ce;}
static int custom_edition_cache_stock_tag(long group) {return resource;}
static int hud_hires_asset_custom_edition(long asset) {return compatible;}
static int hud_hires_asset_ce_only(long asset) {return ce_only;}
static long hud_hires_ce_stock_count_get(void) {return 1;}
static char const *hud_hires_ce_stock_tag(long index) {return "stock";}
static long hud_hires_ce_stock_bitmap(long index) {return 0;}
static int hud_hires_asset_fits(long a,long w,long h) {return 1;}
static unsigned long hud_hires_asset_sprites(long a) {return 0;}
#define scenario_definition_get(i) (&scenario)
static void hud_hires_map_loaded(long const *a,long count) {}
GLOBALS
static void hud_hires_sprite_bitmaps_forget(void) {hires_sprite_bitmap_count=0;}
PRODUCTION
int main(void) {
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==1 && hires_bitmaps[0].stock_custom_edition);
    assert(upscale_bitmap_count==0);
    assert(ce_stock_bitmap_count==1);
    resource=0; /* Embedded custom art under a stock name. */
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==0 && upscale_bitmap_count==0);
    assert(ce_stock_bitmap_count==0);
    resource=1; compatible=0; /* Resource sheet with incompatible layout. */
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==0);
    ce=0; resource=0; /* Xbox registration remains independent of CE eligibility. */
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==1 && !hires_bitmaps[0].stock_custom_edition);
    assert(upscale_bitmap_count>0);
    assert(ce_stock_bitmap_count==0);
    ce_only=1;
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==0);
    ce_only=0;
    ce=1; resource=1; compatible=1;
    hud_hires_tags_loaded(0);
    assert(hires_bitmap_count==1 && upscale_bitmap_count==0);
    return 0;
}
'''.replace('GLOBALS', globals_).replace('PRODUCTION', body))


def test_ce_meter_recipe_retains_thresholds_and_is_reproducible():
    assets = ROOT / 'port/assets/hud'
    entries = json.loads((assets / 'ce_layout.json').read_text())['assets']
    assert {entry['name'] for entry in entries} == {'ce_hud_unit_meters', 'ce_hud_ammo_meters'}
    for entry in entries:
        generated = build_asset(entry)
        with Image.open(assets / (entry['name'] + '.png')) as committed:
            assert generated.tobytes() == committed.tobytes()
        for cell in entry['cells']:
            with Image.open(assets / (cell['source'] + '.png')) as original:
                source = original.crop(cell['from'])
            rect = [round(v * entry['scale']) for v in cell['to']]
            result = generated.crop(rect)
            expected = source.getchannel('R').resize(result.size, Image.Resampling.NEAREST)
            assert result.getchannel('R').tobytes() == expected.tobytes()
        malformed = copy.deepcopy(entry)
        malformed['cells'][0]['to'][2] = entry['width'] + 1
        with pytest.raises(ValueError):
            build_asset(malformed)


def test_full_size_unit_meters_preserve_screen_coordinates():
    entry = json.loads((ROOT / 'port/assets/hud/ce_layout.json').read_text())['assets'][0]
    # Full-size meters are top-right anchored. CE uses twice the resolution
    # with half the placement scale. Different sprite padding must translate
    # the artwork, never stretch it to the stock pixel bounding box.
    for index, xbox_right, xbox_top, ce_right, ce_top in (
            (0, 122, 34, 262, 132), (2, 176, 65, 258, 198)):
        cell = entry['cells'][index]
        for edge in (0, 2):
            assert cell['from'][edge] / 8 - xbox_right == cell['to'][edge] / 2 - ce_right / 2
        for edge in (1, 3):
            assert cell['from'][edge] / 8 - xbox_top == cell['to'][edge] / 2 - ce_top / 2


def test_ce_prompt_guard_preserves_authored_offsets(tmp_path):
    source = (ROOT / 'port/linux/game/hud_hires_tags.c').read_text()
    body = function(source, 'ce_hud_buttons_stock')
    run_c(tmp_path, r'''
#include <assert.h>
#include <stddef.h>
#define TRUE 1
#define FALSE 0
#define NONE -1
#define NUMBEROF(a) (sizeof(a)/sizeof(a[0]))
typedef int boolean;
struct icon_hud_element_definition {short sequence_index,width_offset;
    struct {short x,y;} offset; unsigned long color;char frame_rate;unsigned char flags;short text_index;};
static int ce=1;
static int custom_edition_cache_tags_loaded(void) {return ce;}
PRODUCTION
int main(void) {
    struct icon_hud_element_definition icon={0,-6,{-2,-5},0,0,2,0};
    assert(ce_hud_buttons_stock(&icon,0));
    icon.offset.x++;
    assert(!ce_hud_buttons_stock(&icon,0));
    icon.offset.x--; icon.sequence_index=5;
    assert(!ce_hud_buttons_stock(&icon,0));
    icon.sequence_index=0; icon.text_index=99;
    assert(!ce_hud_buttons_stock(&icon,0));
    icon.text_index=0;
    assert(ce_hud_buttons_stock(&icon,0));
    assert(!ce_hud_buttons_stock(&icon,-1));
    assert(!ce_hud_buttons_stock(&icon,18));
    ce=0;
    assert(ce_hud_buttons_stock(&icon,0));
    return 0;
}
'''.replace('PRODUCTION', body))
