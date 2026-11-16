#include "harness.h"
#define _object_invisible_bit 0
#define _object_mask_all 255
#define NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS 4
#define MODERN_SHADOW_WORLD 1
#define MODERN_SHADOW_BOTH 3
typedef struct {float v[12];} real_matrix4x3;
struct object_datum {
 int definition_index;
 struct {unsigned flags;float bounding_sphere_radius;real_point3d bounding_sphere_center;
  struct {unsigned long size;} node_matrices;unsigned char region_permutations[2];
  int forced_shader_permutation_index,parent_object_index;} object;
} objects[8];
struct object_iterator {int index;};
struct object_definition {struct {struct {int index;} model;} object;} definitions[8];
struct model {struct {int count;} nodes;} models[8];
static real_matrix4x3 poses[8];
static int submitted[8],views_used[8];
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
 CHECK(object_casters(&origin,2,owners,NONE),"player, NPC and dropped models collected");
 CHECK(submitted[0] && submitted[1] && views_used[0]==1 && views_used[1]==1,
       "local body and any held weapon cast into world while excluded from first-person map");
 CHECK(submitted[2] && submitted[3] && submitted[4] && views_used[2]==3 && views_used[3]==3 && views_used[4]==3,
       "NPC, its held weapon and a dropped weapon cast into both receiver maps");
 CHECK(!submitted[5] && !submitted[6] && !submitted[7],"hidden, non-model and distant objects do not cast");
 owners[0]=owners[1]=NONE;memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE) && views_used[0]==3 && views_used[1]==3,
       "third-person and remote player weapons retain full coverage");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,2),"owner-specific map collects remaining casters");
 CHECK(!submitted[2] && submitted[3] && submitted[0] && submitted[1] && submitted[4],
       "only exact owner is omitted; held weapon, other bodies and dropped items still cast");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,0x10002) && submitted[2],"recycled datum cannot exclude a new owner");
 memset(submitted,0,sizeof(submitted));
 CHECK(object_casters(&origin,2,owners,NONE) && submitted[2],"external light still receives owner geometry");
 objects[3].object.node_matrices.size=0;
 CHECK(!object_casters(&origin,2,owners,NONE),"invalid held pose falls back rather than generating malformed silhouettes");
 return 0;
}
