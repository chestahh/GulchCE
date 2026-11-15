#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
typedef int boolean;
typedef float real;
typedef struct {real x,y,z;} real_point3d;
typedef struct {real x,y;} real_point2d;
typedef struct {short x0,y0,x1,y1;} rectangle2d;
typedef struct {real alpha,red,green,blue;} real_argb_color;
#define NONE (-1)
#define FALSE 0
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define PIN(v,l,h) ((v)<(l)?(l):((v)>(h)?(h):(v)))
#define _interface_bitmap_multiplayer_hud 0
struct player_datum {long unit_index;wchar_t name[16];struct {int machine_index;} network_player_data;};
static struct player_datum player={1,L"Enemy",{0}};
static struct {struct {real_point3d position;rectangle2d viewport_bounds;} camera;struct {int world_to_view;} frustum;} render;
static struct {struct {real active_camouflage;} unit;} unit;
struct font_header {short ascending_height,descending_height;};
struct bitmap_data {struct {short y;} registration_point;};
static struct font_header font={10,2};
static struct bitmap_data bitmap={{3}};
static int visible=1,projected=1,rays,draws,font_id=1,mcc;
static real distance_value=5;
static real_point3d ray_head;
static rectangle2d drawn_bounds;
static struct player_datum *player_get(long n){(void)n;return &player;}
static long hud_get_font_index(void){return font_id;}
static void unit_get_head_position(long n,real_point3d *out){(void)n;out->x=5;out->y=0;out->z=7;}
static real distance3d(real_point3d const *a,real_point3d const *b){(void)a;(void)b;return distance_value;}
#define unit_get(n) ((void)(n), &unit)
static boolean hud_player_name_in_sight(long n,real_point3d const *head){(void)n;rays++;ray_head=*head;return visible;}
static void matrix4x3_transform_point(int *matrix,real_point3d const *in,real_point3d *out){(void)matrix;assert(in->z>7.29f && in->z<7.31f);*out=*in;}
#define render_camera_view_to_screen(camera,frustum,in,out) ((void)(camera),(void)(frustum),(void)(in),(out)->x=100,(out)->y=100,projected)
#define fast_ftol(v) ((long)(v))
static long interface_get_tag_index(int n){return n;}
static struct bitmap_data *bitmap_group_get_bitmap_from_sequence(long n,int a,int b){(void)n;(void)a;(void)b;return &bitmap;}
static int mcc_cache_tags_loaded(void){return mcc;}
static real mcc_hud_bitmap_scale(long n){(void)n;return 2;}
static real custom_edition_hud_bitmap_scale(long n){(void)n;return 1;}
static struct font_header *font_definition_get(long n){(void)n;return &font;}
static void hud_player_name_color(boolean ally,real_argb_color *c){(void)ally;memset(c,0,sizeof(*c));}
static void draw_string_set_draw_mode(long a,int b,int c,int d,real_argb_color *color){(void)a;(void)b;(void)c;(void)d;(void)color;}
static real hud_player_name_scale(void){return 1;}
static void rasterizer_text_set_scale(real a,real b,real c){(void)a;(void)b;(void)c;}
static void rasterizer_draw_unicode_string(rectangle2d *b,void *p,void *q,int n,wchar_t *s){(void)p;(void)q;(void)n;assert(!wcscmp(s,L"Enemy"));drawn_bounds=*b;draws++;}
static int network_voice_machine_speaking(int n){(void)n;return 0;}
static void draw_unicode_string_compute_bounds(rectangle2d *b,wchar_t *s,rectangle2d *t,rectangle2d *c){(void)s;*t=*b;*c=*b;}
static real draw_unicode_string_capital_middle(rectangle2d *b,wchar_t *s){(void)s;return b->y0;}
static void network_voice_draw_icon(rectangle2d *i,int n,real a){(void)i;(void)n;(void)a;}
#include "under_test.inc"
int main(void){
    int i,r,frames;
    projected=0;for(i=0;i<300;i++)hud_draw_player_name(1,0,0,10);
#ifdef BASELINE
    assert(rays==300);
#else
    assert(rays==0);
#endif
    assert(draws==0);printf("300 rejected projections: %d rays, %d draws\n",rays,draws);
    projected=1;rays=draws=0;
    /* Repeated frames within one tick still observe changing visibility. */
    for(frames=30;frames<=240;frames*=2){
        r=rays;for(i=0;i<frames;i++){visible=i%2;hud_draw_player_name(1,0,1,10);}
        assert(rays-r==frames);
    }
    assert(ray_head.z==7);r=rays;visible=1;
    distance_value=10;hud_draw_player_name(1,0,0,10);assert(rays==r);
    distance_value=5;unit.unit.active_camouflage=.6f;hud_draw_player_name(1,0,0,10);assert(rays==r);
    unit.unit.active_camouflage=0;visible=0;hud_draw_player_name(1,1,1,10);assert(rays==r);
    assert(drawn_bounds.y1==95);
    mcc=1;hud_draw_player_name(1,1,1,10);assert(rays==r && drawn_bounds.y1==92);
    font_id=NONE;hud_draw_player_name(1,0,0,10);assert(rays==r);
    assert(draws==227);
    printf("Drawable enemy frames keep fresh rays at 30/60/120/240 Hz; 227 identical draws\n");return 0;
}

