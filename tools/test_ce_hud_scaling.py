"""Authored CE HD scaling, separate from Xbox redraws and MCC's canvas."""
from pathlib import Path
from harness import function
from test_hud_button_style import run_c

ROOT = Path(__file__).resolve().parents[1]


def test_draw_flags_and_known_map_numbers_stay_in_ce(tmp_path):
    source = (ROOT / 'port/linux/game/custom_edition_hud_draw.c').read_text()
    body = '\n'.join(function(source, name) for name in [
        'custom_edition_hud_bitmap_scale', 'custom_edition_hud_number_scale'])
    flags = (ROOT / 'port/linux/game/custom_edition_hud_scale.h').read_text()
    run_c(tmp_path, r'''
#include <assert.h>
typedef float real;
typedef int boolean;
#define FALSE 0
#define NONE -1
#define _custom_edition_behaviour_hud_number_scale 7
static int ce, legacy, reads;
static struct {unsigned short flags;} bitmap;
static int custom_edition_cache_tags_loaded(void) {return ce;}
static int custom_edition_cache_relies_on(int flag) {assert(flag==7);return legacy;}
#define bitmap_group_get(index) (reads++, &bitmap)
''' + flags + body + r'''
int main(void) {
    int p,b;
    const unsigned short bits[4]={0,0x10,0x80,0x90};
    const float expected[2][4]={{1,.5f,.5f,.25f},{.5f,.25f,.5f,.25f}};
    ce=1;
    for(p=0;p<2;p++) for(b=0;b<4;b++) {
        bitmap.flags=bits[b];
        assert(custom_edition_hud_number_scale(1,p*4)==expected[p][b]);
        assert(custom_edition_hud_bitmap_scale(1)==expected[0][b]);
    }
    /* Bigass-style metric correction already supplies the halving. */
    legacy=1;bitmap.flags=0x10;
    assert(custom_edition_hud_number_scale(1,4)==.5f);
    assert(custom_edition_hud_number_scale(1,0)==.5f);
    /* Xbox, MCC, unload/failed loads: never look up a CE bitmap. */
    ce=0;reads=0;
    assert(custom_edition_hud_number_scale(1,4)==1);
    assert(custom_edition_hud_bitmap_scale(1)==1);
    ce=1;
    assert(custom_edition_hud_number_scale(NONE,4)==1);
    assert(custom_edition_hud_bitmap_scale(NONE)==1);
    assert(reads==0);
    legacy=0;bitmap.flags=0;
    assert(custom_edition_hud_number_scale(1,0)==1);
    return 0;
}
''')
