"""Exercise the actual shoulder camera in a fake world, without game assets."""
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("fault", [None, "zoom", "collision", "shoulder", "reticle"])
def test_shoulder_camera(tmp_path, fault):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("a C compiler is required")
    code = (ROOT / "port/linux/game/third_person.c").read_text()
    code = re.sub(r'^#include[^\n]*', '', code, flags=re.M)
    if fault == "zoom":
        code = code.replace("player_control_get_zoom_level(local_player_index) != NONE", "0")
    elif fault == "collision":
        code = code.replace("hard_fraction * length < (was_active ? 0.10f : 0.14f)", "0")
    elif fault == "shoulder":
        code = code.replace("+ state->side * right.", "- state->side * right.")
    elif fault == "reticle":
        code = code.replace("screen.x - center_x", "0")
    stub = r'''
#include <math.h>
#include <stddef.h>
#include <string.h>
typedef int boolean;
typedef float real;
typedef struct {real i,j,k;} real_vector3d;
typedef struct {real x,y,z;} real_point3d;
typedef struct {real x,y;} real_point2d;
typedef struct {real yaw,pitch;} real_euler_angles2d;
#define _pi 3.14159265358979323846f
#define DEGREES_TO_RADIANS(d) ((d)*_pi/180.0f)
#define HALO_KEYBOARD_FREE_LOOK 20
static unsigned long held;
static unsigned long halo_keyboard_actions(short p){return p==0?held:0;}
static void euler_angles2d_from_vector3d(real_euler_angles2d *a,real_vector3d const *v){a->yaw=atan2f(v->j,v->i);a->pitch=asinf(v->k);}
static void vector3d_from_euler_angles2d(real_vector3d *v,real_euler_angles2d const *a){v->i=cosf(a->yaw)*cosf(a->pitch);v->j=sinf(a->yaw)*cosf(a->pitch);v->k=sinf(a->pitch);}
static void observer_up_from_forward(real_vector3d const *f,real_vector3d *u){real_euler_angles2d a;euler_angles2d_from_vector3d(&a,f);a.pitch+=_pi/2;vector3d_from_euler_angles2d(u,&a);}
typedef struct {short x,y;} point2d;
typedef struct {short x0,y0,x1,y1;} rectangle2d;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_LOCAL_PLAYERS 4
#define FLAG(n) (1u<<(n))
#define TEST_FLAG(v,n) ((v)&FLAG(n))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define PIN(v,a,b) MIN(MAX(v,a),b)
enum {_director_perspective_first_person, _object_dead_bit=0,
_collision_test_front_facing_surfaces_bit, _collision_test_back_facing_surfaces_bit,
_collision_test_structure_bit, _collision_test_objects_bit,
_collision_test_objects_sight_blocking_flags=64, _collision_test_for_projectiles_flags=1024};
struct unit_datum {
 struct {long parent_object_index; unsigned damage_flags;} object;
 struct {int current_zoom_level, desired_zoom_level; real_vector3d aiming_vector;} unit;
};
struct render_camera {real_point3d position; real_vector3d forward,up; rectangle2d window_bounds;};
struct collision_result {real t; real_point3d point;};
static struct {short local_player_index; struct render_camera camera; int frustum;} render;
static struct unit_datum body={{NONE,0},{NONE,NONE,{1,0,0}}};
static unsigned long changes;
static int enabled, cinematic, scripted, perspective, inhibited, zoom=NONE, unit_id=1;
static int *director_camera_scripted=&scripted;
static real wall=1, prop=1, soft_wall=1;
static int smooth=1;
static real_point3d eye={0,0,0};
static void third_person_muzzle_target(short p,real_point3d *t){(void)p;(void)t;}
static int on_screen=1,left_shoulder;
static double setting_distance=.85,setting_height=.12,setting_offset=.24;
static real main_get_seconds_elapsed(void){return 1.0f/60;}
double config_real(char const *name){
 if(strstr(name,"distance"))return setting_distance;
 if(strstr(name,"height"))return setting_height;
 return setting_offset;
}
static void vector_from_points3d(real_point3d const *a,real_point3d const *b,real_vector3d *v){v->i=b->x-a->x;v->j=b->y-a->y;v->k=b->z-a->z;}

int config_boolean(char const *name){return strstr(name,"smooth")?smooth:(strstr(name,"left_shoulder")?left_shoulder:enabled);}
unsigned long config_changes(void){return changes;}
static int cinematic_in_progress(void){return cinematic;}
static int director_get_perspective(short p){(void)p;return perspective;}
static int director_inhibited_facing(short p){(void)p;return inhibited;}
static int player_control_get_zoom_level(short p){(void)p;return zoom;}
static long player_control_get_unit_index(short p){(void)p;return unit_id;}
static struct unit_datum *unit_try_and_get(long i){(void)i;return &body;}
static struct unit_datum *unit_get(long i){(void)i;return &body;}
static void unit_get_camera_position(long i,real_point3d *p){(void)i;*p=eye;}
static void cross_product3d(real_vector3d const *a,real_vector3d const *b,real_vector3d *v){
 v->i=a->j*b->k-a->k*b->j;v->j=a->k*b->i-a->i*b->k;v->k=a->i*b->j-a->j*b->i;
}
static real magnitude3d(real_vector3d const *v){return sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);}
static void normalize3d(real_vector3d *v){real l=magnitude3d(v);v->i/=l;v->j/=l;v->k/=l;}
static void scale_vector3d(real_vector3d const *v,real s,real_vector3d *o){o->i=v->i*s;o->j=v->j*s;o->k=v->k*s;}
static void point_from_line3d(real_point3d const *p,real_vector3d const *v,real s,real_point3d *o){
 o->x=p->x+v->i*s;o->y=p->y+v->j*s;o->z=p->z+v->k*s;
}
static int collision_test_pill_new(unsigned long f,real_point3d const *p,real_vector3d const *v,real r,long i,struct collision_result *c){
 (void)f;(void)p;(void)v;(void)i;c->t=r>.08f?MIN(wall,soft_wall):wall;return c->t<1;
}
static int collision_test_vector(unsigned long f,real_point3d const *p,real_vector3d const *v,long i,struct collision_result *c){
 (void)i;c->t=f==1024?.01f:prop;point_from_line3d(p,v,c->t,&c->point);return c->t<1;
}
static int render_camera_world_to_screen(struct render_camera const *c,int const *f,real_point3d const *p,real_point2d *s){
 (void)c;(void)f;s->x=350;s->y=260;return on_screen && p->x==10;
}
'''
    checks = r'''
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static struct render_camera fresh(void){struct render_camera c={{0,0,0},{1,0,0},{0,0,1},{0,0,640,480}};return c;}
static struct render_camera settle(short player){int i;struct render_camera c;for(i=0;i<180;i++){c=fresh();third_person_camera(player,&c);}return c;}
int main(void){
 struct render_camera c=fresh();point2d p={320,240};
 third_person_camera(0,&c);CHECK(!third_person_active(0)&&c.position.x==0);
 enabled=1;changes++;c=settle(0);
 CHECK(third_person_active(0)&&c.position.x<-.8f&&c.position.y<-.2f&&c.position.z>.1f);
 CHECK(body.unit.aiming_vector.i==1&&eye.x==0);
 render.camera=c;CHECK(third_person_reticle(&p,.5f));CHECK(p.x==380&&p.y==280);
 on_screen=0;CHECK(!third_person_reticle(&p,1));on_screen=1;
 zoom=0;c=fresh();third_person_camera(0,&c);CHECK(!third_person_active(0)&&c.position.x==0);zoom=NONE;
 body.unit.current_zoom_level=0;third_person_camera(0,&c);CHECK(!third_person_active(0));body.unit.current_zoom_level=NONE;
 body.unit.desired_zoom_level=0;third_person_camera(0,&c);CHECK(!third_person_active(0));body.unit.desired_zoom_level=NONE;
 body.object.parent_object_index=2;third_person_camera(0,&c);CHECK(!third_person_active(0));body.object.parent_object_index=NONE;
 body.object.damage_flags=1;third_person_camera(0,&c);CHECK(!third_person_active(0));body.object.damage_flags=0;
 cinematic=1;third_person_camera(0,&c);CHECK(!third_person_active(0));cinematic=0;
 scripted=1;third_person_camera(0,&c);CHECK(!third_person_active(0));scripted=0;
 perspective=1;third_person_camera(0,&c);CHECK(!third_person_active(0));perspective=0;
 unit_id=NONE;third_person_camera(0,&c);CHECK(!third_person_active(0));unit_id=1;
 c=settle(0);wall=.5f;c=fresh();third_person_camera(0,&c);CHECK(third_person_active(0)&&c.position.x>-.45f&&c.position.x<-.35f);
 wall=.05f;c=fresh();third_person_camera(0,&c);CHECK(!third_person_active(0)&&c.position.x==0);wall=1;
 prop=.5f;c=settle(0);CHECK(third_person_active(0)&&c.position.x>-.4f);prop=1;
 c=settle(1);CHECK(third_person_active(1));
 left_shoulder=1;changes++;c=fresh();third_person_camera(1,&c);CHECK(c.position.y<0);
 c=settle(1);CHECK(c.position.y>.23f);
 setting_distance=1.5;setting_height=.3;setting_offset=.4;changes++;c=settle(1);
 CHECK(c.position.x < -1.49f && c.position.y>.39f && c.position.z>.29f);
 wall=.05;c=fresh();third_person_camera(1,&c);CHECK(!third_person_active(1));wall=1;
 c=fresh();third_person_camera(1,&c);CHECK(third_person_active(1)&&c.position.x>-.4f);
 c=settle(1);CHECK(third_person_active(1));
 setting_distance=NAN;setting_height=100;setting_offset=-1;changes++;c=settle(1);
 CHECK(c.position.x>-.86f&&c.position.x<-.84f&&c.position.y>.23f&&c.position.y<.25f);
 setting_distance=.3;setting_height=0;setting_offset=0;changes++;c=settle(1);
 CHECK(third_person_active(1)&&c.position.x>-.301f&&c.position.x<-.299f);
 soft_wall=0;c=settle(1);CHECK(third_person_active(1));soft_wall=1;
 smooth=0;changes++;setting_distance=1.0;changes++;c=fresh();third_person_camera(1,&c);
 CHECK(c.position.x < -.99f);
 left_shoulder=0;setting_offset=.24;changes++;c=fresh();third_person_camera(1,&c);CHECK(c.position.y<-.23f);
 third_person_reset();CHECK(!third_person_active(1));c=settle(1);

 c=settle(0);held=1UL<<HALO_KEYBOARD_FREE_LOOK;
 CHECK(third_person_free_look_input(0,1.0f,.2f));
 CHECK(!third_person_free_look_input(1,1.0f,.2f));
 c=fresh();third_person_camera(0,&c);CHECK(c.forward.j>.7f&&c.forward.k>.1f);
 CHECK(body.unit.aiming_vector.i==1&&body.unit.aiming_vector.j==0);
 CHECK(!third_person_reticle(&p,1));
 wall=0;c=fresh();third_person_camera(0,&c);
 CHECK(!third_person_active(0)&&c.forward.i==1&&c.forward.j==0&&c.up.k==1);
 CHECK(third_person_free_look_input(0,.1f,0));wall=1;c=settle(0);
 held=0;CHECK(!third_person_free_look_input(0,0,0));c=fresh();third_person_camera(0,&c);
 CHECK(c.forward.i==1&&c.forward.j==0); /* sharp release */
 smooth=1;changes++;c=settle(0);held=1UL<<HALO_KEYBOARD_FREE_LOOK;
 CHECK(third_person_free_look_input(0,1.0f,0));c=fresh();third_person_camera(0,&c);
 held=0;CHECK(!third_person_free_look_input(0,0,0));c=fresh();third_person_camera(0,&c);
 CHECK(c.forward.j>0&&c.forward.j<.85f);c=settle(0);CHECK(c.forward.j==0);
 held=1UL<<HALO_KEYBOARD_FREE_LOOK;CHECK(third_person_free_look_input(0,1.0f,0));
 zoom=0;CHECK(!third_person_free_look_input(0,.5f,0));zoom=NONE;
 c=fresh();third_person_camera(0,&c);CHECK(c.forward.j==0);
 CHECK(third_person_free_look_input(0,1.0f,0));unit_id=2;
 CHECK(!third_person_free_look_input(0,.5f,0));unit_id=1;held=0;
 third_person_reset();c=settle(1);
 third_person_camera(0,NULL);CHECK(!third_person_active(0)&&third_person_active(1));
 CHECK(!third_person_active(-1)&&!third_person_active(4));
 enabled=0;changes++;c=fresh();third_person_camera(1,&c);CHECK(!third_person_active(1));
 return 0;
}
'''
    source = tmp_path / "shoulder.c"
    source.write_text(stub + code + checks)
    exe = tmp_path / ("shoulder.exe" if sys.platform == "win32" else "shoulder")
    flags = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if sys.platform == "win32" else ["-lm"]
    compiled = subprocess.run([compiler, str(source), *flags, "-o", str(exe)], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    ran = subprocess.run([str(exe)], timeout=10)
    assert (ran.returncode != 0) == bool(fault), ran.returncode


@pytest.mark.parametrize("fault", [None, "fov", "effects", "visibility"])
def test_perspective_paths(tmp_path, fault):
    from harness import function
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("a C compiler is required")
    fov = function((ROOT / "port/linux/game/view_fov.c").read_text(), "render_fov_vertical")
    effects = function((ROOT / "source/effects/effects.c").read_text(), "effect_location_get_next_instance")
    visibility = function((ROOT / "source/render/render_particles.c").read_text(), "local_player_is_first_person")
    if fault == "fov":
        fov = fov.replace("third_person_active(local_player_index)", "0")
    elif fault == "effects":
        effects = effects.replace("!third_person_active(effect->local_player_index)", "1")
    elif fault == "visibility":
        visibility = visibility.replace("third_person_active(local_player_index)", "0")
    code = r'''
#include <math.h>
#include <stddef.h>
typedef int boolean;
typedef float real;
#define NONE -1
#define FALSE 0
#define TRUE 1
#define MAXIMUM_LOCAL_PLAYERS 4
#define _pi 3.14159265358979323846f
#define FLAG(b) (1u<<(b))
#define TEST_FLAG(v,b) ((v)&FLAG(b))
#define match_assert(a,b,c) ((void)0)
#define TAG_BLOCK_GET_ELEMENT(a,b,t) (&fake_seat)
enum {_effect_camera_mode_independent_of_camera_mode, _effect_camera_mode_first_person_only,
_effect_camera_mode_third_person_only, _effect_camera_mode_both, _effect_location_first_person_bit=15,
_director_perspective_first_person=0, _unit_seat_gunner_bit=1};
struct effect_datum {short local_player_index;};
struct effect_location_datum {long next_instance_location_index; int node_designator;};
struct player_datum {long unit_index;};
struct unit_datum {struct {long parent_object_index;} object;struct {int parent_seat_index;} unit;long definition_index;};
struct unit_seat {unsigned flags;};
static struct unit_seat fake_seat;
static struct unit_datum body={{NONE},{NONE},0};
static struct player_datum player={1};
static int active, players=1;
static real reticle_scales[4],authored_vertical[4];
static struct effect_location_datum locations[2]={{1,0},{NONE,32768}};
static boolean third_person_active(short p){return p==0&&active;}
static real render_fov_adjust(short p,real f){(void)p;(void)f;return 2.0f;}
static int local_player_count(void){return players;}
static struct effect_location_datum *effect_location_get(long n){return &locations[n];}
static int director_get_perspective(short p){(void)p;return 0;}
static long local_player_get_player_index(short p){return p;}
static struct player_datum *player_get(long n){(void)n;return &player;}
static struct unit_datum *unit_get(long n){(void)n;return &body;}
''' + fov + effects + visibility + r'''
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int main(void){
 struct effect_datum e={0};long index=0;
 CHECK(render_fov_vertical(0,1.0f)==2.0f&&reticle_scales[0]<1&&authored_vertical[0]==1);
 CHECK(local_player_is_first_person(0));
 CHECK(effect_location_get_next_instance(&e,&index,3)==&locations[1]);
 active=1;index=0;
 CHECK(render_fov_vertical(0,1.0f)==1.0f&&reticle_scales[0]==1&&authored_vertical[0]==0);
 CHECK(!local_player_is_first_person(0));
 CHECK(effect_location_get_next_instance(&e,&index,3)==&locations[0]);
 CHECK(effect_location_get_next_instance(&e,&index,3)==NULL);
 index=0;CHECK(effect_location_get_next_instance(&e,&index,1)==&locations[1]);
 index=0;CHECK(effect_location_get_next_instance(&e,&index,2)==&locations[0]);
 active=0;index=0;
 CHECK(render_fov_vertical(0,1.0f)==2.0f&&reticle_scales[0]<1);
 CHECK(effect_location_get_next_instance(&e,&index,3)==&locations[1]);
 players=2;index=0;CHECK(effect_location_get_next_instance(&e,&index,3)==&locations[0]);
 return 0;
}
'''
    source = tmp_path / "perspective.c"
    source.write_text(code)
    exe = tmp_path / ("perspective.exe" if sys.platform == "win32" else "perspective")
    flags = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if sys.platform == "win32" else ["-lm"]
    compiled = subprocess.run([compiler, str(source), *flags, "-o", str(exe)], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    ran = subprocess.run([str(exe)], timeout=10)
    assert (ran.returncode != 0) == bool(fault), ran.returncode


def test_offline_muzzle_origin(tmp_path):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("a C compiler is required")
    actual = (ROOT / "port/linux/game/third_person_weapon.c").read_text()
    actual = re.sub(r'^#include[^\n]*', '', actual, flags=re.M)
    code = r'''
#include <math.h>
#include <stddef.h>
#include <string.h>
typedef float real;
typedef int boolean;
typedef struct {real x,y,z;} real_point3d;
typedef struct {real i,j,k;} real_vector3d;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_LOCAL_PLAYERS 4
#define MAX(a,b) ((a)>(b)?(a):(b))
#define TEST_FLAG(v,b) ((v)&(1u<<(b)))
enum {_game_connection_local=0,_object_dead_bit=0,_collision_test_for_projectiles_flags=1};
enum {_weapon_trigger_uses_weapon_origin_bit=5,_weapon_trigger_projectiles_cannot_be_aimed_bit=11,
_unit_fires_from_camera_bit=0,_trigger_charging=2,_trigger_charged=3,_trigger_spewing=6};
struct unit_datum {struct {long parent_object_index;unsigned damage_flags;} object;struct {short current_weapon_index;real_vector3d aiming_vector;} unit;long definition_index;};
struct tag_block {long count;void *address;};
struct weapon_trigger_definition {unsigned flags;real_point3d first_person_weapon_offset;struct tag_block firing_effects;real spew_time;};
struct weapon_firing_effect_definition {struct {long index;} effects[3];};
struct effect_definition {struct tag_block locations;};
#define EFFECT_DEFINITION_TAG 1
static char effect_marker[32]="primary trigger1";
static struct effect_definition effect={{1,effect_marker}};
static struct weapon_firing_effect_definition firing={{{0},{-1},{-1}}};
static void *tag_get(int group,long n){(void)group;(void)n;return &effect;}
struct weapon_definition {struct {struct tag_block triggers;} weapon;};
struct weapon_datum {long definition_index;struct {struct {int state;short firing_effect_index;} triggers[2];} weapon;};
struct unit_definition {struct {unsigned flags;} unit;};
static struct weapon_trigger_definition trigger_defs[2];
static struct weapon_definition definition={{{2,trigger_defs}}};
static struct weapon_datum weapon;
static struct unit_definition unit_def={{1}};
#define TAG_BLOCK_GET_ELEMENT(block,n,type) ((type *)(block)->address+(n))
static struct weapon_datum *weapon_get(long n){(void)n;return &weapon;}
static struct weapon_definition *weapon_definition_get(long n){(void)n;return &definition;}
static struct unit_definition *unit_definition_get(long n){(void)n;return &unit_def;}
static real_vector3d up_axis={0,0,1},left_axis={0,1,0};
static real_vector3d const *global_up3d=&up_axis,*global_left3d=&left_axis;
static void cross_product3d(real_vector3d const *a,real_vector3d const *b,real_vector3d *v){*v=(real_vector3d){a->j*b->k-a->k*b->j,a->k*b->i-a->i*b->k,a->i*b->j-a->j*b->i};}
static real dot_product3d(real_vector3d const *a,real_vector3d const *b){return a->i*b->i+a->j*b->j+a->k*b->k;}
static void unit_adjust_projectile_ray(long u,real_point3d *p,real_vector3d *v,real *speed,int adjust,int aim){(void)u;(void)aim;(void)v;*speed=0;if(adjust){p->y=0;p->z=0;}}
struct object_marker {struct {real_point3d position;} matrix;};
struct collision_result {real t;real_point3d point;};
static struct unit_datum unit={{NONE,0},{0,{1,0,0}},0};
static int connection,active=1,zoom=NONE,has_marker=1,blocked;
static real target_distance=10, muzzle_side;
static int scene;
static real_point3d last_long_origin;
static int marker_trigger;
static short game_connection(void){return connection;}
static boolean third_person_active(short p){return p==0&&active;}
static long player_control_get_unit_index(short p){return p==0?1:NONE;}
static short player_control_get_zoom_level(short p){(void)p;return zoom;}
static struct unit_datum *unit_try_and_get(long i){return i==1?&unit:NULL;}
static struct unit_datum *unit_get(long i){return unit_try_and_get(i);}
static long unit_inventory_get_weapon(long i,short n){(void)i;(void)n;return 2;}
static int object_get_marker_by_name(long w,char const *n,struct object_marker *m,short count){
 (void)w;(void)count;marker_trigger=strstr(n,"secondary")!=NULL;m->matrix.position=(real_point3d){1,muzzle_side,marker_trigger?-.4f:(!strcmp(n,"primary trigger1")?-.3f:-.2f)};return has_marker;
}
static void unit_get_camera_position(long u,real_point3d *p){(void)u;*p=(real_point3d){0,0,0};}
static void vector_from_points3d(real_point3d const *a,real_point3d const *b,real_vector3d *v){*v=(real_vector3d){b->x-a->x,b->y-a->y,b->z-a->z};}
static real magnitude3d(real_vector3d const *v){return sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);}
static real normalize3d(real_vector3d *v){real l=magnitude3d(v);if(l){v->i/=l;v->j/=l;v->k/=l;}return l;}
static void scale_vector3d(real_vector3d const *v,real s,real_vector3d *o){*o=(real_vector3d){v->i*s,v->j*s,v->k*s};}
static void point_from_line3d(real_point3d const *p,real_vector3d const *v,real t,real_point3d *o){*o=(real_point3d){p->x+v->i*t,p->y+v->j*t,p->z+v->k*t};}
static int collision_test_vector(unsigned long f,real_point3d const *p,real_vector3d const *v,long u,struct collision_result *c){
 (void)f;(void)u;
 if(scene && v->i>0){
  real plane=scene==2?.5f:1.5f;
  real t=(plane-p->x)/v->i;
  if(t>=0&&t<=1&&(scene==2||p->z+v->k*t<-.1f)){
   c->t=t;point_from_line3d(p,v,t,&c->point);return 1;
  }
 }
 if(magnitude3d(v)>100){last_long_origin=*p;
 if(v->i>0 && target_distance>p->x){c->t=(target_distance-p->x)/v->i;point_from_line3d(p,v,c->t,&c->point);return 1;}
 return 0;}
 if(blocked){c->t=.5f;point_from_line3d(p,v,.5f,&c->point);return 1;}
 return 0;
}
''' + actual + r'''
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int main(void){
 real_point3d origin={0,0,0},target={10,0,0};real_vector3d forward={1,0,0};
 third_person_muzzle_shot(2,1,0,&origin,&forward);
 CHECK(origin.x==1&&origin.z==-.2f&&forward.k>0);
 CHECK(fabsf(origin.z+forward.k*(9/forward.i))<.0001f);
 connection=1;origin=(real_point3d){0,0,0};forward=(real_vector3d){1,0,0};
 third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x==0&&forward.k==0);
 connection=2;third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x==0);
 connection=0;active=0;third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x==0);
 active=1;zoom=0;third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x==0);
 zoom=NONE;has_marker=0;third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x==0);
 has_marker=1;third_person_muzzle_shot(2,1,5,&origin,&forward);CHECK(origin.x==0);
 blocked=1;third_person_muzzle_shot(2,1,0,&origin,&forward);CHECK(origin.x>.48f&&origin.x<.5f);
 blocked=0;third_person_muzzle_target(0,&target);CHECK(target.x==10);
 CHECK(last_long_origin.z==-.2f&&marker_trigger==0);
 weapon.weapon.triggers[0].state=_trigger_charged;third_person_muzzle_target(0,&target);
 CHECK(last_long_origin.z==-.4f&&marker_trigger==1);
 weapon.weapon.triggers[0].state=_trigger_charging;third_person_muzzle_target(0,&target);CHECK(marker_trigger==0);
 weapon.weapon.triggers[0].state=0;
 trigger_defs[0].first_person_weapon_offset.y=-.1f;third_person_muzzle_target(0,&target);
 CHECK(fabsf(target.y+.1f)<.0001f);trigger_defs[0].first_person_weapon_offset.y=0;
 target_distance=.5f;origin=(real_point3d){0,0,0};forward=(real_vector3d){1,0,0};
 CHECK(third_person_muzzle_shot(2,1,0,&origin,&forward));CHECK(forward.i==1&&forward.k==0);
 target_distance=10;
 trigger_defs[0].firing_effects=(struct tag_block){1,&firing};
 origin=(real_point3d){1,0,0};forward=(real_vector3d){1,0,0};
 CHECK(third_person_muzzle_shot(2,1,0,&origin,&forward));CHECK(origin.z==-.3f);
 strcpy(effect_marker,"smoke");origin=(real_point3d){1,0,0};forward=(real_vector3d){1,0,0};
 CHECK(third_person_muzzle_shot(2,1,0,&origin,&forward));CHECK(origin.z==-.2f);
 weapon.weapon.triggers[0].firing_effect_index=9;
 third_person_muzzle_target(0,&target);CHECK(last_long_origin.z==-.2f);
 weapon.weapon.triggers[0].firing_effect_index=0;trigger_defs[0].firing_effects.count=0;
 scene=1;muzzle_side=.25f;
 origin=(real_point3d){1,0,0};forward=(real_vector3d){1,0,0};
 CHECK(third_person_muzzle_shot(2,1,0,&origin,&forward));
 CHECK(origin.x==1&&origin.y==.25f);
 third_person_muzzle_target(0,&target);CHECK(target.x==1.5f&&target.z<-.1f);
 CHECK(fabsf((target.y-origin.y)*forward.i-(target.x-origin.x)*forward.j)<.0001f);
 muzzle_side=-.25f;third_person_muzzle_target(0,&target);CHECK(target.x==1.5f&&target.y<0);
 scene=2;origin=(real_point3d){1,0,0};forward=(real_vector3d){1,0,0};
 CHECK(third_person_muzzle_shot(2,1,0,&origin,&forward));
 CHECK(origin.x<.5f&&origin.x>.48f&&forward.i>0);
 third_person_muzzle_target(0,&target);CHECK(fabsf(target.x-.5f)<.0001f);
 scene=0;muzzle_side=0;
 connection=1;target=(real_point3d){10,0,0};third_person_muzzle_target(0,&target);CHECK(target.x==10);
 return 0;
}
'''
    source = tmp_path / "muzzle.c"
    source.write_text(code)
    exe = tmp_path / ("muzzle.exe" if sys.platform == "win32" else "muzzle")
    flags = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if sys.platform == "win32" else ["-lm"]
    compiled = subprocess.run([compiler, str(source), *flags, "-o", str(exe)], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    assert subprocess.run([str(exe)], timeout=10).returncode == 0

@pytest.mark.parametrize("fault", [False, True])
def test_muzzle_bsp_repair_excludes_shooter(tmp_path, fault):
    from harness import function
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("a C compiler is required")
    actual = function((ROOT / "source/objects/objects.c").read_text(), "object_force_inside_bsp_ignoring")
    if fault:
        actual = actual.replace("&object->object.position, ignore_object_index,", "&object->object.position, NONE,")
    code = r'''
#include <stddef.h>
typedef int boolean;
typedef struct {float x,y,z;} real_point3d;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_COLLISION_USER_STACK_DEPTH 8
#define _collision_test_for_projectiles_flags 1
#define match_assert(a,b,c) ((void)0)
struct location {short cluster_index;};
struct object_datum {struct {real_point3d position;struct location location;} object;};
struct collision_result {real_point3d point;struct location location;};
static struct object_datum projectile={{{1,0,-.2f},{0}}};
static int global_current_collision_user_depth=1,global_current_collision_users[8],wall,translated;
static struct object_datum *object_get(long i){(void)i;return &projectile;}
static int collision_test_line(int flags,real_point3d const *a,real_point3d const *b,long ignore,struct collision_result *c){
 (void)flags;(void)a;(void)b;
 c->location.cluster_index=0;c->point=(real_point3d){.1f,0,0};
 if(ignore!=7)return 1; /* the eye-to-barrel line crosses the shooter */
 if(wall){c->point=(real_point3d){.5f,0,-.1f};return 1;}return 0;
}
static void object_translate(long i,real_point3d const *p,struct location const *l){(void)i;projectile.object.position=*p;projectile.object.location=*l;translated++;}
static void object_compute_node_matrices(long i){(void)i;}
''' + actual + r'''
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int main(void){real_point3d eye={0,0,0};
 CHECK(object_force_inside_bsp_ignoring(2,&eye,7));
 CHECK(translated==0&&projectile.object.position.x==1);
 wall=1;CHECK(object_force_inside_bsp_ignoring(2,&eye,7));
 CHECK(translated==1&&projectile.object.position.x==.5f);
 wall=0;CHECK(object_force_inside_bsp_ignoring(2,&eye,NONE));
 CHECK(translated==2&&projectile.object.position.x==.1f);
 CHECK(global_current_collision_user_depth==1);return 0;
}
'''
    source = tmp_path / "repair.c"
    source.write_text(code)
    exe = tmp_path / ("repair.exe" if sys.platform == "win32" else "repair")
    flags = ["--target=i686-pc-windows-msvc", "-fuse-ld=lld"] if sys.platform == "win32" else ["-lm"]
    compiled = subprocess.run([compiler, str(source), *flags, "-o", str(exe)], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    assert (subprocess.run([str(exe)], timeout=10).returncode != 0) == fault
