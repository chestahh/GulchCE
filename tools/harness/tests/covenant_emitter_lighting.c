#include "harness.h"
#include <math.h>
#include <string.h>
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PIN(a,b,c) MIN(MAX(a,b),c)
enum { _object_type_biped=0, _object_type_weapon=2, _object_type_scenery=6,
 _object_mask_biped=1, _object_mask_weapon=4, _object_mask_scenery=64,
 _object_invisible_bit=0, _object_shield_depleted_bit=2,
 MAXIMUM_REGIONS_PER_OBJECT=8, MAXIMUM_WEAPONS_PER_UNIT=4 };
typedef struct { real red,green,blue; } real_rgb_color;
typedef struct { real scale; real_vector3d forward,left,up; real_point3d position; } real_matrix4x3;
struct object_marker { real_matrix4x3 matrix; };
struct object_datum { long definition_index; struct {
 long flags,damage_flags,regions_destroyed_flags,parent_object_index;
 short type; struct { int size; } node_matrices;
 real body_vitality,maximum_body_vitality,shield_vitality,recent_shield_damage;
 unsigned char region_permutations[8]; real_rgb_color outgoing_change_colors[4];
 } object; };
struct unit_datum { struct { long flags; } object; struct { short current_weapon_index; } unit; };
struct block { int count; };
struct model_region { char name[32]; };
struct model { struct block regions; };
struct object_definition { struct { struct { long index; } model; } object; };
static struct object_datum objects[10];
static struct unit_datum parent;
static struct model model={{1}};
static struct model_region fake_region={"shield"};
static struct object_definition definition;
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&fake_region)
static const char *names[10];
static real now,marker_x=1;
static int count=1,shield_on=1,fuel_on,stationary_on,allocs,deletes,disconnects,moves,missing,fail,marker_missing;
static long inventory=0;
static int ammo_on,first_person;
#define weapon_datum object_datum
static struct { int active,connected; real radius; real_rgb_color color; real_point3d position; } storage[64];
static real_vector3d forward={1,0,0};
static const real_vector3d *global_forward3d=&forward;
struct observer_result { real_point3d position; };
static struct observer_result camera;
struct object_iterator { long index; };
static void object_iterator_new(struct object_iterator *it,int mask,int flags) { it->index=-1; }
static void *object_iterator_next(struct object_iterator *it) { return ++it->index<count?&objects[it->index]:NULL; }
static struct object_datum *object_try_and_get(long i) { return i>=0&&i<count?&objects[i]:NULL; }
static struct object_datum *weapon_try_and_get(long i) { return object_try_and_get(i); }
static struct unit_datum *unit_try_and_get(long i) { return i==42?&parent:NULL; }
static long unit_inventory_get_weapon(long i,short slot) { return inventory; }
static struct object_definition *object_definition_get(long i) { return &definition; }
static struct model *model_definition_get(long i) { return &model; }
static const char *tag_get_name(long i) { return names[i]; }
static int object_get_marker_by_name(long i,const char *name,struct object_marker *m,int count)
{ memset(m,0,sizeof(*m));m->matrix.forward.i=1;m->matrix.up.k=1;m->matrix.position.x=marker_x;return !marker_missing; }
static int first_person_weapon_adjust_light(long i,const char *n,real_point3d *p,real_vector3d *f,real_vector3d *u)
{ memset(p,0,sizeof(*p));memset(f,0,sizeof(*f));memset(u,0,sizeof(*u));p->x=5;f->j=1;u->k=1;return first_person&&!marker_missing; }
static short local_player_get_next(short i) { return i==NONE?0:NONE; }
static const struct observer_result *observer_get_camera(short i) { return &camera; }
static real distance3d(const real_point3d *a,const real_point3d *b) { return (real)fabs(a->x-b->x); }
static long game_time_get(void) { return (long)(now*30); }
real render_interpolation_game_time_sec(long ticks) { return now; }
int config_boolean(const char *name)
{
 if (!strcmp(name,"display.jackal_shield_lighting")) return shield_on;
 if (!strcmp(name,"display.fuel_rod_lighting")) return fuel_on;
 if (!strcmp(name,"display.covenant_shield_lighting")) return stationary_on;
 if (!strcmp(name,"display.ammo_counter_lighting")) return ammo_on;
 return 0;
}
long light_port_glow_definition_get(void) { return missing?NONE:99; }
static long light_new_unattached(long d,long o,short n,const real_point3d *p,const real_vector3d *v,real scale)
{ if(fail)return NONE;storage[allocs].active=storage[allocs].connected=1;return 100+allocs++; }
boolean light_port_glow_set(long l,long o,const real_rgb_color *c,real r)
{ int i=l-100;if(i<0||i>=allocs||!storage[i].active)return FALSE;storage[i].color=*c;storage[i].radius=r;return TRUE; }
boolean light_port_glow_move(long l,const real_point3d *p)
{ storage[l-100].position=*p;storage[l-100].connected=1;moves++;return TRUE; }
static void light_delete(long l) { storage[l-100].active=0;deletes++; }
static void light_disconnect_from_map(long l)
{ CHECK(storage[l-100].connected,"double disconnect");storage[l-100].connected=0;disconnects++; }
void plasma_occlusion_submit(long l,long r) {}
#include "under_test.inc"
static void frame(real dt) { now+=dt;covenant_emitter_lighting_update(); }
int main(int argc,char **argv)
{
 int i,old;real slow,fast;struct emitter_sample sample;
 for(i=0;i<10;i++) {
  objects[i].definition_index=i;names[i]="characters\\jackal\\jackal";
  objects[i].object.node_matrices.size=sizeof(real_matrix4x3);
  objects[i].object.parent_object_index=NONE;objects[i].object.maximum_body_vitality=1;
  objects[i].object.body_vitality=objects[i].object.shield_vitality=1;
  objects[i].object.outgoing_change_colors[2].blue=1;
 }
 CHECK(emitter_sample(0,&sample)&&sample.kind==0&&sample.radius==2.6f,"minor shield profile");
 names[0]="characters\\jackal\\jackal major";objects[0].object.outgoing_change_colors[2].red=1;
 CHECK(emitter_sample(0,&sample)&&sample.color.red==1,"major uses authored colour");
 objects[0].object.body_vitality=0;
 CHECK(emitter_sample(0,&sample)&&sample.strength==0,"dead Jackal has no shield illumination");
 objects[0].object.body_vitality=1;
 marker_x=24;CHECK(emitter_sample(0,&sample)&&sample.strength>0&&sample.strength<0.4f,"distance boundary fades before admission cutoff");
 marker_x=26;CHECK(emitter_sample(0,&sample)&&sample.strength==0,"distant emitter stays stock");marker_x=1;
 objects[0].object.type=_object_type_scenery;names[0]="scenery\\c_field_generator\\c_field_generator";
 CHECK(!emitter_sample(0,&sample),"stationary setting independent");stationary_on=1;
 objects[0].object.maximum_body_vitality=objects[0].object.body_vitality=0;
 CHECK(emitter_sample(0,&sample)&&sample.strength>0&&sample.kind==2,"indestructible shield base eligible");
 objects[0].object.region_permutations[0]=1;
 CHECK(emitter_sample(0,&sample)&&sample.strength==0,"shield-off permutation overrides marker");
 objects[0].object.region_permutations[0]=0;
 objects[0].object.type=_object_type_weapon;names[0]="weapons\\fuel rod gun\\fuel rod";
 CHECK(!emitter_sample(0,&sample),"fuel setting independent");fuel_on=1;
 CHECK(emitter_sample(0,&sample)&&sample.kind==1,"dropped rods remain luminous");
 objects[0].object.parent_object_index=42;inventory=1;
 CHECK(!emitter_sample(0,&sample),"holstered weapon rejected");inventory=0;
 CHECK(emitter_sample(0,&sample),"held current weapon eligible");
 names[0]="weapons\\fuel rod gun\\hunter fuel rod";
 CHECK(!emitter_sample(0,&sample),"no phantom rods on model-less Hunter gun");
 names[0]="characters\\jackal\\jackal";objects[0].object.type=0;objects[0].object.parent_object_index=NONE;
 objects[0].object.maximum_body_vitality=objects[0].object.body_vitality=1;
 covenant_emitter_lighting_update();
 for(i=0;i<6;i++)frame(1.0f/30);
 slow=emitters[0].strength;covenant_emitter_lighting_frame_end();
 CHECK(disconnects==1&&storage[0].color.blue==0,"dark and disconnected between simulation ticks");
 covenant_emitter_lighting_forget();now=0;covenant_emitter_lighting_update();
 frame(1.0f/120);CHECK(emitters[0].strength>0,"fade advances between simulation ticks");
 for(i=1;i<24;i++)frame(1.0f/120);
 fast=emitters[0].strength;CHECK(fabs(slow-fast)<0.0001,"frame-rate independent easing");
 old=allocs;frame(0);CHECK(allocs==old&&emitters[0].strength==fast,"split views do not advance fade");
 marker_x=2;frame(1.0f/120);CHECK(storage[allocs-1].position.x==2,"rendered marker follows light");
 covenant_emitter_lighting_submitted(emitters[0].light,3);
 CHECK(covenant_emitter_lighting_selected(3),"submitted emitter selects soft path without projectile setting");
 CHECK(covenant_emitter_lighting_radius(emitters[0].light,.1f)==2.6f,"bounds cover full light");
 objects[0].object.shield_vitality=0;frame(1.0f/120);
 CHECK(emitters[0].strength>0&&emitters[0].strength<fast,"collapse fades instead of popping");
 for(i=0;i<120;i++)frame(1.0f/120);
 CHECK(!emitters[0].active,"collapsed light released");
 objects[0].object.shield_vitality=1;frame(.01f);frame(.01f);
 CHECK(emitters[0].strength>0&&emitters[0].strength<0.2f,"recovered shield eases back in");
 now=-1;old=deletes;covenant_emitter_lighting_update();CHECK(deletes==old+1,"clock rollback discards old light");
 objects[0].object.shield_vitality=1;count=10;covenant_emitter_lighting_update();frame(.1f);
 old=allocs;frame(.1f);CHECK(allocs==old,"stable bounded identities");
 for(i=0,old=0;i<8;i++)old+=emitters[i].active;CHECK(old==8,"eight-light cap");
 shield_on=0;covenant_emitter_lighting_frame_end();
 for(i=0;i<8;i++)CHECK(!emitters[i].active,"disable releases all owned lights");
 shield_on=1;count=1;marker_missing=1;old=allocs;frame(.1f);CHECK(allocs==old,"missing marker remains stock");
 marker_missing=0;missing=1;frame(.1f);frame(.1f);CHECK(allocs==old,"missing template remains stock");
 missing=0;fail=1;frame(.1f);frame(.1f);CHECK(allocs==old,"allocation failure remains stock");
 fail=0;frame(.1f);frame(.1f);count=0;old=deletes;frame(.1f);CHECK(deletes==old+1,"deletion releases owned light");
 covenant_emitter_lighting_forget();CHECK(!covenant_emitter_lighting_selected(3),"map forget clears identities");
 shield_on=fuel_on=stationary_on=0;count=1;old=allocs;frame(.1f);
 CHECK(allocs==old,"all options off allocate nothing");
 stationary_on=1;objects[0].object.type=_object_type_scenery;names[0]="scenery\\c_field_generator\\c_field_generator";
 frame(.01f);frame(.01f);
 CHECK(covenant_emitter_lighting_occlusion_ignore(emitters[0].light)==0,"stationary shield cannot occlude itself");
 CHECK(covenant_emitter_lighting_occlusion_ignore(NONE)==NONE,"unowned light has no occlusion exclusion");
 stationary_on=0;CHECK(covenant_emitter_lighting_occlusion_ignore(emitters[0].light)==NONE,"disabled source has no exclusion");
 objects[0].object.type=_object_type_weapon;names[0]="weapons\\assault rifle\\assault rifle";
 objects[0].object.parent_object_index=42;inventory=0;marker_x=1;
 CHECK(!emitter_sample(0,&sample),"ammo setting independent and off");ammo_on=1;
 CHECK(emitter_sample(0,&sample)&&sample.kind==3&&sample.radius<1&&sample.strength<.3f,"subtle counter profile");
 CHECK(fabs(sample.position.x-.82f)<.0001&&fabs(sample.position.z-.027744f)<.0001,"third person counter offset");
 first_person=1;
 CHECK(emitter_sample(0,&sample)&&sample.position.x==5&&fabs(sample.position.y+.18f)<.0001,"first person follows rotated rendered marker");
 CHECK(fabs(sample.position.z-.025f)<.0001,"first person has separate offset");
 inventory=1;CHECK(!emitter_sample(0,&sample),"holstered counter rejected");inventory=0;
 objects[0].object.parent_object_index=NONE;CHECK(!emitter_sample(0,&sample),"dropped counter is off");
 objects[0].object.parent_object_index=42;marker_missing=1;CHECK(!emitter_sample(0,&sample),"missing counter marker stays stock");marker_missing=0;
 first_person=0;frame(.01f);frame(.1f);ammo_on=0;old=deletes;frame(.01f);
 CHECK(deletes>old,"counter disable releases owned light");
 return 0;
}
