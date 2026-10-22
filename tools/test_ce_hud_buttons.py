"""Exercise the production low-resolution CE controller resource reader."""
from pathlib import Path
from test_hud_button_style import run_c

ROOT = Path(__file__).resolve().parents[1]


def test_original_sprites_bounds_animation_and_cleanup(tmp_path):
    source = (ROOT / 'port/linux/game/custom_edition_hud_buttons.h').read_text()
    source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
    run_c(tmp_path, r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;typedef unsigned char byte;typedef float real;typedef uint32_t pixel32;
typedef struct {short x,y;}point2d;
typedef struct {short x0,y0,x1,y1;}rectangle2d;
typedef struct {float x0,x1,y0,y1;}real_rectangle2d;
struct bitmap_group_sprite {short bitmap_index,pad;uint32_t unused;real_rectangle2d bounds;float x,y;};
struct bitmap_data {void *base_address;short width,height;};
struct icon_hud_element_definition {short sequence_index,frame_rate,width_offset;point2d offset;int flags;pixel32 color;};
struct cache_file_source {void *context;int (*read)(void *,uint32_t,uint32_t,void *);uint32_t size;};
struct resource_map_item {uint32_t data_offset,size;char const *name;};
struct resource_map {struct cache_file_source *source;int item_count;struct resource_map_item *items;};
#define TRUE 1
#define FALSE 0
#define csmemset memset
#define csmemcpy memcpy
#define _hud_anchor_bottom_left 2
static byte file[1024+65536];static int live,reads,draws,available=1,fail,players=1;
static long ticks;static float drawn_x,drawn_height,drawn_width;static pixel32 drawn_color;
static point2d drawn_point;
static int read_bytes(void *ctx,uint32_t off,uint32_t size,void *dst) {
    reads++;if(fail || off>sizeof(file) || size>sizeof(file)-off)return 0;
    memcpy(dst,file+off,size);return 1;
}
static struct cache_file_source src={0,read_bytes,sizeof(file)};
static struct resource_map_item item={0,512,"ui\\hud\\bitmaps\\hud_msg_icons_sm"};
static struct resource_map resource={&src,1,&item};
static struct resource_map const *custom_edition_cache_bitmap_resources(void){return available?&resource:0;}
static long game_time_get(void){return ticks;}
static int local_player_count(void){return players;}
static int draw_unicode_string_capital_middle(rectangle2d const *r,void const *s){return 80;}
static struct bitmap_data *bitmap_2d_new(int w,int h,int m,int f){
    struct bitmap_data *b=calloc(1,sizeof(*b));assert(w==128&&h==128&&!m&&f==11);
    b->width=w;b->height=h;b->base_address=malloc(65536);live++;return b;
}
static void bitmap_delete(struct bitmap_data *b){free(b->base_address);free(b);live--;}
static int rasterizer_bitmap_new(struct bitmap_data *b){return 1;}
static void rasterizer_bitmap_changed(struct bitmap_data *b){assert(((byte *)b->base_address)[0]==73);}
static void hud_draw_bitmap_direct(struct bitmap_data *b,int anchor,point2d *point,
    real_rectangle2d *clip,float scale,float angle,pixel32 color,int crosshair){
    assert(anchor==2 && !crosshair);drawn_x=clip->x0;drawn_color=color;draws++;
    drawn_point=*point;drawn_height=(clip->y1-clip->y0)*128*scale;
    drawn_width=(clip->x1-clip->x0)*128*scale;
}
PRODUCTION
static void put(unsigned off,uint32_t n){memcpy(file+off,&n,4);}
static void fixture(void){
    struct bitmap_group_sprite s={0};
    memset(file,0,sizeof(file));put(84,1);put(88,108);put(96,1);put(100,256);
    put(108+52,2);put(108+56,172);
    s.bounds.x1=16.0f/128;s.bounds.y1=20.0f/128;memcpy(file+172,&s,32);
    s.bounds.x0=0.25f;s.bounds.x1=0.375f;memcpy(file+204,&s,32);
    put(260,0x00800080);put(264,1);put(268,11);put(280,1024);put(284,65536);
    file[1024]=73;
}
int main(void){
    struct icon_hud_element_definition icon={0,10,-6,{-2,-5},2,0xabcdef01};
    rectangle2d cursor={100,0,0,100};int n;
    fixture();assert(ce_hud_button_draw(&icon,&cursor,0));
    assert(live==1 && drawn_x==0 && drawn_color==icon.color && cursor.x0==121);
    assert(drawn_point.x==104 && drawn_point.y==93 && drawn_height==26.0f);
    assert(drawn_width>20.7f && drawn_width<20.9f);
    n=reads;ticks=10;assert(ce_hud_button_draw(&icon,&cursor,0));assert(drawn_x==0.25f && reads==n);
    ticks=20;assert(ce_hud_button_draw(&icon,&cursor,0));assert(drawn_x==0);
    available=0;assert(!ce_hud_button_draw(&icon,&cursor,0));available=1;
    icon.sequence_index=-1;assert(!ce_hud_button_draw(&icon,&cursor,0));icon.sequence_index=1;
    assert(!ce_hud_button_draw(&icon,&cursor,0));icon.sequence_index=0;
    ce_hud_buttons_forget();assert(!live);ce_hud_buttons_forget();
    put(88,0xfffffff0);assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();
    fixture();put(108+52,0xffffffff);assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();
    fixture();put(172+8,0x7fc00000);ticks=0;assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();
    fixture();put(280,sizeof(file)-1);assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();
    fixture();put(268,14);assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();
    fixture();fail=1;assert(!ce_hud_button_draw(&icon,&cursor,0));ce_hud_buttons_forget();fail=0;
    fixture();players=2;icon.flags=0;assert(ce_hud_button_draw(&icon,&cursor,0x11223344));
    assert(drawn_height>19.4f && drawn_height<19.6f && drawn_point.y==89);
    assert(drawn_color==0x11223344);ce_hud_buttons_forget();assert(!live);
    /* Narrow buttons and wide trigger/stick cells retain their aspect ratio
       and stay centered, instead of all being forced into a face-sized box. */
    {
        int sizes[][2]={{13,15},{68,20},{75,20},{65,20},{71,20},{4,4}};int i;
        for(i=0;i<6;i++){
            struct bitmap_group_sprite s={0};
            fixture();s.bounds.x1=sizes[i][0]/128.0f;s.bounds.y1=sizes[i][1]/128.0f;
            memcpy(file+172,&s,32);ticks=0;players=1;cursor.x0=100;
            assert(ce_hud_button_draw(&icon,&cursor,0));
            assert(drawn_point.x==104 && drawn_point.y-drawn_height*0.5f<=80.0f);
            assert(drawn_point.y-drawn_height*0.5f>79.0f);
            assert(cursor.x0>=drawn_point.x+drawn_width-4.0f);
            ce_hud_buttons_forget();
        }
    }
    return 0;
}
'''.replace('PRODUCTION', source))
