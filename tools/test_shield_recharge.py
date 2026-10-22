"""Actual recharge module: state, queued-draw identity, arm limits and lifecycle."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module")
def recharge_tool(tmp_path_factory):
    cc = shutil.which('clang')
    if not cc: pytest.skip('clang required')
    work = tmp_path_factory.mktemp('shield-recharge')
    source = (ROOT/'port/linux/game/shield_recharge.inl').read_text()
    source = re.sub(r'^#include.*$', '', source, flags=re.M)
    preamble = r'''
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef float real;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PIN(x,a,b) MAX(a,MIN(x,b))
#define TEST_FLAG(x,b) ((x)&(1U<<(b)))
enum {_object_type_biped=0,_object_dead_bit=0,_object_shield_depleted_bit=1,_object_shield_over_charging_bit=2,_object_shield_charging_bit=3};
struct object_datum {struct {int type;unsigned damage_flags;int shield_stun_ticks;float shield_vitality;} object;};
struct shader {int id;};
struct shield_look_plasma {struct shader shader;real offset_amount;};
struct {struct shield_look_plasma body,arms;boolean sprinkles;} shield_looks[2];
static short shield_look_count=2;
struct render_model_effect {struct shader *modifier_shader;struct {real const *values;} modifier_animation;};
struct {real game_time_sec;} global_frame_parameters;
static struct object_datum objects[140];
static int enhanced=1,arms=1;
static unsigned long changes=1;
static real body_values[140][4],arm_values[2][4];
static struct object_datum *object_try_and_get(long n) {return n>=0 && n<140?&objects[n]:NULL;}
static int shield_look_enabled(void) {return enhanced;}
unsigned long config_changes(void) {return changes;}
int config_boolean(char const *name) {(void)name;return arms;}
static real viewmodel_shield_offset_amount(real const *values,real amount) {
 return values==arm_values[0] || values==arm_values[1]?MIN(amount,.004f):amount;
}
'''
    main = r'''
static struct render_model_effect prepare(long n,real level,real time,int side) {
 struct render_model_effect e;
 objects[n].object.shield_vitality=level;
 global_frame_parameters.game_time_sec=time;
 e.modifier_shader=side?&shield_looks[0].arms.shader:&shield_looks[0].body.shader;
 e.modifier_animation.values=side?arm_values[side-1]:body_values[n];
 shield_recharge_prepare(n,&e,side!=0);return e;
}
static void draw(struct render_model_effect *e,real *intensity,real *offset) {
 shield_recharge_draw(e->modifier_shader,e->modifier_animation.values,intensity,offset);
}
int main(int argc,char **argv) {
 struct render_model_effect e,a,b;
 struct shield_look_plasma original;
 real i,o,low,high;
 int n;
 assert(argc==2);
 for(n=0;n<140;n++)objects[n].object.damage_flags=8;
 shield_looks[0].body.offset_amount=.0125f;shield_looks[0].arms.offset_amount=.03f;
 original=shield_looks[0].body;shield_recharge_reset();
 e=prepare(0,.01f,1.f,0);i=o=0;draw(&e,&i,&o);assert(i>0 && i<.1f);low=o;
 e=prepare(0,.5f,1.5f,0);i=o=0;draw(&e,&i,&o);assert(i>.2f && i<.5f);assert(o>low);low=o;
 e=prepare(0,.999f,2.f,0);i=o=0;draw(&e,&i,&o);assert(o>low && o<.0145f);high=o;
 e=prepare(0,1.f,2.01f,0);i=o=0;draw(&e,&i,&o);assert(o>=high);
 e=prepare(0,1.f,2.26f,0);i=o=0;draw(&e,&i,&o);assert(o>0 && o<high*.6f);
 e=prepare(0,1.f,2.52f,0);i=.123f;o=.002f;draw(&e,&i,&o);assert(i==.123f && o==.002f);assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));
 assert(!memcmp(&original,&shield_looks[0].body,sizeof(original)));assert(body_values[0][0]==0);
 shield_recharge_reset();e=prepare(0,.5f,3.f,0);
 if(!strcmp(argv[1],"damage"))objects[0].object.shield_stun_ticks=30;
 else if(!strcmp(argv[1],"dead"))objects[0].object.damage_flags|=1;
 else if(!strcmp(argv[1],"depleted"))objects[0].object.damage_flags|=2;
 else if(!strcmp(argv[1],"overcharging"))objects[0].object.damage_flags|=4;
 else if(!strcmp(argv[1],"not-charging"))objects[0].object.damage_flags=0;
 else if(!strcmp(argv[1],"classic")){enhanced=0;changes++;}
 else if(!strcmp(argv[1],"reset"))shield_recharge_reset();
 else if(!strcmp(argv[1],"sprinkles")) {
  real enhanced_i, enhanced_o, last_i=1.f;
  shield_looks[0].sprinkles=1;shield_recharge_reset();
  e=prepare(0,.999f,4.f,0);i=o=0;draw(&e,&i,&o);assert(i<.471f);
  e=prepare(0,1.f,4.1f,0);
  for(n=1;n<15;n++) {
   real t=4.1f+n/30.f;
   e=prepare(0,1.f,t,0);i=o=0;draw(&e,&i,&o);
   assert(i>0 && i<last_i);last_i=i;
   enhanced_i=.47f*shield_recharges[0].fade;
   enhanced_o=.0125f*1.15f*shield_recharges[0].fade;
   assert(i>enhanced_i && i<=enhanced_i*1.75f);
   assert(o==enhanced_o);
   shield_looks[0].sprinkles=0;e=prepare(0,1.f,t,0);i=o=0;draw(&e,&i,&o);
   assert(i==enhanced_i && o==enhanced_o);
   shield_looks[0].sprinkles=1;
  }
  e=prepare(0,1.f,4.61f,0);i=.123f;o=.002f;draw(&e,&i,&o);assert(i==.123f && o==.002f);
  assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));
  shield_recharge_reset();a=prepare(0,.5f,5.f,1);i=o=0;draw(&a,&i,&o);assert(o<=.004f);
  arms=0;changes++;assert(!shield_recharge_visible(a.modifier_shader,a.modifier_animation.values));return 0;
 }
 else if(!strcmp(argv[1],"arms")) {
  a=prepare(0,.5f,3.f,1);b=prepare(1,.8f,3.f,2);
  i=o=0;draw(&a,&i,&o);assert(o>0 && o<=.004f);low=o;
  i=o=0;draw(&b,&i,&o);assert(o>low && o<=.004f);
  arms=0;changes++;a=prepare(0,.5f,3.1f,1);assert(!shield_recharge_visible(a.modifier_shader,a.modifier_animation.values));
  e=prepare(0,.5f,3.1f,0);assert(shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));return 0;
 }
 else if(!strcmp(argv[1],"respawn")) {
  a=prepare(0,.5f,3.f,1);b=prepare(1,1.f,3.f,1);
  assert(!shield_recharge_visible(a.modifier_shader,a.modifier_animation.values));return 0;
 }
 else if(!strcmp(argv[1],"time")) {
  global_frame_parameters.game_time_sec=3.1f;assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));
  e=prepare(0,1.f,5.f,0);assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));
  e=prepare(0,.5f,6.f,0);e=prepare(0,1.f,1.f,0);assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));return 0;
 }
 else if(!strcmp(argv[1],"capacity")) {
  shield_recharge_reset();for(n=0;n<140;n++)e=prepare(n,.5f,3.f,0);
  assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));return 0;
 }
 else if(!strcmp(argv[1],"unrelated")) {
  e.modifier_shader=&shield_looks[1].body.shader;i=.123f;o=.004f;draw(&e,&i,&o);assert(i==.123f && o==.004f);return 0;
 }
 else assert(!strcmp(argv[1],"full"));
 if(!strcmp(argv[1],"reset")) {assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));return 0;}
 e=prepare(0,!strcmp(argv[1],"full")?1.5f:.5f,3.1f,0);
 assert(!shield_recharge_visible(e.modifier_shader,e.modifier_animation.values));
 i=.123f;o=.004f;draw(&e,&i,&o);assert(i==.123f && o==.004f);
 return 0;
}
'''
    (work/'test.c').write_text(preamble+source+main)
    output=work/('test.exe' if os.name=='nt' else 'test')
    flags=['-m32','-std=gnu89','-Werror']
    if os.name=='nt': flags+=['--target=i686-pc-windows-msvc','-fuse-ld=lld','-D_CRT_SECURE_NO_WARNINGS']
    result=subprocess.run([cc,*flags,str(work/'test.c'),'-o',str(output)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    return output

@pytest.mark.parametrize('case',['damage','dead','depleted','overcharging','not-charging','classic','reset','arms','respawn','time','capacity','unrelated','full','sprinkles'])
def test_recharge(recharge_tool,case):
    result=subprocess.run([str(recharge_tool),case],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
