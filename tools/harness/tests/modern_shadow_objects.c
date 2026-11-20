#include "harness.h"
#define _object_invisible_bit 0
#define _object_mask_all 255
#define _object_mask_vehicle 2
enum { _object_type_biped, _object_type_weapon, MAXIMUM_WEAPONS_PER_UNIT=4 };
#define MAXIMUM_OBJECTS_PER_MAP 8
#define NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS 4
#define MODERN_SHADOW_WORLD 1
#define MODERN_SHADOW_BOTH 3
typedef struct {float v[12];} real_matrix4x3;
struct object_datum {
 int definition_index;
 struct {unsigned flags;float bounding_sphere_radius;real_point3d bounding_sphere_center;
  struct {unsigned long size;} node_matrices;unsigned char region_permutations[2];
  int forced_shader_permutation_index,parent_object_index,type;} object;
} objects[8];
struct object_iterator {int index;};
struct object_definition {struct {struct {int index;} model;} object;} definitions[8];
struct model {struct {int count;} nodes;} models[8];
static real_matrix4x3 poses[8];
static int submitted[8],views_used[8];
static int vehicle[8],biped[8];
static struct unit_datum {struct {int type;} object; struct {int parent_seat_index,current_weapon_index;} unit;} units[8];
static long inventory[8][MAXIMUM_WEAPONS_PER_UNIT];
static long unit_inventory_get_weapon(long owner,short slot) {return inventory[owner][slot];}
static struct object_datum *object_try_and_get(long i) {return i>=0 && i<8 ? &objects[i] : NULL;}
static struct unit_datum *unit_try_and_get(long i) {return i>=0 && i<8 && biped[i] ? &units[i] : NULL;}
static void *object_try_and_get_and_verify_type(long i,int mask) {
 CHECK(mask==_object_mask_vehicle,"only vehicle owners extend the exception");
 return object_try_and_get(i) && vehicle[i] ? &objects[i] : NULL;
}
static void object_iterator_new(struct object_iterator *i,int mask,int flags) {
 CHECK(mask==_object_mask_all,"all object classes participate");i->index=-1;
}
static struct object_datum *object_iterator_next(struct object_iterator *i) {
 return ++i->index<8 ? &objects[i->index] : NULL;
}
static float distance_squared3d(real_point3d const *a,real_point3d const *b) {
 float d=a->x-b->x;return d*d;
}
static struct object_definition *object_definition_get(int i) {return &definitions[i];}
static struct model *model_definition_get(int i) {return &models[i];}
static real_matrix4x3 *object_get_node_matrices(int i) {return &poses[i];}
static void object_function_values_for_render(int i,float *values) {values[0]=(float)i;}
static int model_caster(struct model *m,real_matrix4x3 *pose,unsigned char *regions,int permutation,float *values,int views) {
 int i=(int)(m-models);
 CHECK(pose==&poses[i] && regions==objects[i].object.region_permutations && values[0]==i,
       "each object uses its own current pose, regions and render functions");
 submitted[i]++;views_used[i]=views;return 1;
}
static int clipping_spot;
static int spot_sphere_visible(real_point3d const *center,float radius) {
 return center->x+radius>=0;
}
static void object_get_render_bounding_sphere(long i,real_point3d *center,float *radius) {
 *center=objects[i].object.bounding_sphere_center; *radius=objects[i].object.bounding_sphere_radius;
}
static void render_interpolation_object_point(long i,real_point3d *center) {(void)i;(void)center;}
#include "under_test.inc"
int main(int argc,char **argv) {
 real_point3d origin={0,0,0};long owners[2]={0,1};int i;
 for(i=0;i<8;i++) {
  objects[i].definition_index=i;definitions[i].object.model.index=i;
  objects[i].object.node_matrices.size=sizeof(real_matrix4x3);models[i].nodes.count=1;
  objects[i].object.parent_object_index=NONE;
 }
 objects[1].object.parent_object_index=0; /* Player's held weapon. */
 objects[3].object.parent_object_index=2; /* NPC's held weapon. */
 objects[5].object.flags=FLAG(_object_invisible_bit);
 definitions[6].object.model.index=NONE;
 objects[7].object.bounding_sphere_center.x=10;
 CHECK(object_casters(&origin,2,owners,NONE,NONE,NONE),"player, NPC and dropped models collected");
 CHECK(submitted[0] && submitted[1] && views_used[0]==1 && views_used[1]==1,
       "local body and any held weapon cast into world while excluded from first-person map");
 CHECK(submitted[2] && submitted[3] && submitted[4] && views_used[2]==3 && views_used[3]==3 && views_used[4]==3,
       "NPC, its held weapon and a dropped weapon cast into both receiver maps");
 CHECK(!submitted[5] && !submitted[6] && !submitted[7],"hidden, non-model and distant objects do not cast");
 owners[0]=owners[1]=NONE;memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE,NONE,NONE) && views_used[0]==3 && views_used[1]==3,
       "third-person and remote player weapons retain full coverage");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,2,NONE,NONE),"owner-specific map collects remaining casters");
 CHECK(!submitted[2] && submitted[3] && submitted[0] && submitted[1] && submitted[4],"ordinary owner exemption");
 biped[2]=1;objects[3].object.type=objects[4].object.type=_object_type_weapon;
 inventory[2][0]=3;inventory[2][1]=4;objects[4].object.parent_object_index=2;
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,2,NONE,NONE) && !submitted[2] && !submitted[3] && submitted[1] && submitted[4],
       "shield exempts its bearer and equipped gun, retaining another biped's gun and a holstered gun");
 CHECK(unshadowed_object(2,3) && !unshadowed_object(NONE,3),"only its own shield exempts the weapon receiver");
 units[2].unit.current_weapon_index=1;
 CHECK(!unshadowed_object(2,3) && unshadowed_object(2,4),"exemption follows a weapon switch");
 objects[4].object.parent_object_index=NONE;
 CHECK(!unshadowed_object(2,4),"a dropped weapon casts even before the inventory slot clears");
 units[2].unit.current_weapon_index=NONE;
 CHECK(!unshadowed_object(2,3),"unarmed Jackal does not exempt stored weapons");
 biped[2]=0;
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,0x10002,NONE,NONE) && submitted[2],"recycled datum cannot exclude a new owner");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE,NONE,NONE) && submitted[2],"external light still receives owner geometry");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE,2,NONE) && !submitted[2] && submitted[0] && submitted[3],
       "turret map omits only its collision chassis, retaining other objects");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE,NONE,NONE) && submitted[2],"external light restores rendered chassis");
 vehicle[2]=1;biped[0]=1;units[0].unit.parent_seat_index=0;
 objects[0].object.parent_object_index=2;
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE,2,3) && !submitted[2] && !submitted[3] && submitted[0] && submitted[1] && submitted[4],
       "a grenade attached to the vehicle keeps its rider, held weapon and nearby object casting");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,2,NONE,NONE),"occupied vehicle collection completes");
 CHECK(!submitted[0] && !submitted[1] && !submitted[2] && submitted[3] && submitted[4],
       "own engine light exempts seated rider and held gear but not arbitrary attachments or nearby items");
 CHECK(unshadowed_object(2,0) && unshadowed_object(2,1),"same exception is available to body and gear receivers");
 CHECK(!unshadowed_object(NONE,0) && !unshadowed_object(0x10002,1),"external and recycled light owners retain rider shadows");
 vehicle[2]=0;
 CHECK(!unshadowed_object(2,0) && !unshadowed_object(2,1),"non-vehicle exceptions stay exact-owner only");
 vehicle[2]=1;units[0].unit.parent_seat_index=NONE;
 CHECK(!unshadowed_object(2,1),"unseated child is not a passenger");
 units[0].unit.parent_seat_index=0;objects[0].object.parent_object_index=NONE;
 CHECK(!unshadowed_object(2,0) && !unshadowed_object(2,1),"dismount immediately restores rider and gear shadows");
 objects[0].object.parent_object_index=1;
 CHECK(!unshadowed_object(2,0),"malformed parent cycle terminates without excluding an unrelated object");
 objects[0].object.parent_object_index=NONE;
 objects[3].object.node_matrices.size=0;
 CHECK(!object_casters(&origin,2,owners,NONE,NONE,NONE),"invalid held pose falls back rather than generating malformed silhouettes");
 return 0;
}
