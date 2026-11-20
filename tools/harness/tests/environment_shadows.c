#include "harness.h"
#include <stddef.h>
#define REAL_MAX 3.4e38f
#define NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS 4
#define MODERN_SHADOW_WORLD 1
#define _object_invisible_bit 0
typedef struct real_matrix4x3 { float v[16]; } real_matrix4x3;
static struct object_datum {
 int definition_index;
 struct { unsigned flags; struct { unsigned size; } node_matrices;
 unsigned char region_permutations[2]; short forced_shader_permutation_index;
 long first_child_object_index, next_object_index; } object;
} objects[4];
static struct object_definition { struct { struct { int index; } model; } object; } definitions[4];
static struct model { struct { int count; } nodes; } models[4];
static real_matrix4x3 poses[4];
static struct { int rasterizer_target; struct { real_matrix4x3 view_to_world; float projection_matrix[4][4]; } frustum; } global_window_parameters;
static struct { int draw_environment_shadows; } rasterizer_debug_options;
static const void *cached_bsp;
static int mode, begins, ends, gpu_ok=1, begin_ok=1, caster_ok=1, submitted[4], soft_mode;
static int setting(void) { return mode; }
static struct object_datum *object_try_and_get(long i) { return i>=0 && i<4 ? &objects[i] : NULL; }
static struct object_datum *object_get(long i) { CHECK(i>=0 && i<4,"validated datum"); return &objects[i]; }
static struct object_definition *object_definition_get(int i) { return &definitions[i]; }
static struct model *model_definition_get(int i) { return &models[i]; }
static real_matrix4x3 *object_get_node_matrices(long i) { return &poses[i]; }
static void object_function_values_for_render(long i,float *v) { v[0]=(float)i; }
static void matrix_to_gl(const real_matrix4x3 *m,float *out) { memcpy(out,m->v,64); }
static int model_caster(const struct model *model,const real_matrix4x3 *pose,const unsigned char *regions,short permutation,const float *values,int views) {
 int i=(int)(model-models);
 CHECK(pose==poses+i && regions==objects[i].object.region_permutations && values[0]==i,"correct rendered owner pose and material");
 CHECK(views==MODERN_SHADOW_WORLD,"environmental caster uses world pose");
 submitted[i]++;return caster_ok;
}
static int modern_environment_begin(const float *frame,float radius,int soft) { begins++;soft_mode=soft;return begin_ok; }
static int modern_environment_end(int complete,const float *camera,const float *projection,const float *color) {
 ends++;return complete && gpu_ok;
}
static long sun_owner = NONE;
static int sun_owns(long object) { return object == sun_owner; }
#include "under_test.inc"
int main(int argc,char **argv) {
 real_matrix4x3 frame={{0}};float color[3]={.5f,.6f,.7f};int i,remaining;
 for(i=0;i<4;i++) {
  objects[i].definition_index=i;definitions[i].object.model.index=i;models[i].nodes.count=1;
  objects[i].object.node_matrices.size=sizeof(real_matrix4x3);
  objects[i].object.first_child_object_index=objects[i].object.next_object_index=NONE;
 }
 cached_bsp=&frame;rasterizer_debug_options.draw_environment_shadows=1;
 CHECK(!modern_shadows_environment(0,&frame,color,1) && !begins,"Off uses stock without GPU setup");
 mode=1;global_window_parameters.rasterizer_target=1;
 CHECK(!modern_shadows_environment(0,&frame,color,1) && !begins,"non-primary target remains stock");
 global_window_parameters.rasterizer_target=0;
 sun_owner=0;
 CHECK(modern_shadows_environment(0,&frame,color,1) && !begins,"completed sun pass owns the family before either old map");
 sun_owner=NONE;
 objects[0].object.first_child_object_index=1;objects[1].object.first_child_object_index=2;
 CHECK(modern_shadows_environment(0,&frame,color,1),"complete replacement owns projection");
 CHECK(submitted[0]==1 && submitted[1]==1 && submitted[2]==1 && !submitted[3],"only root and held/attached descendants");
 gpu_ok=0;
 CHECK(!modern_shadows_environment(0,&frame,color,1),"receiver failure keeps stock fallback");
 gpu_ok=1;begin_ok=0;i=ends;
 CHECK(!modern_shadows_environment(0,&frame,color,1) && ends==i,"allocation or budget rejection keeps stock");
 begin_ok=1;caster_ok=0;i=ends;
 CHECK(!modern_shadows_environment(0,&frame,color,1) && ends==i+1,"incomplete caster still closes GPU transaction");
 caster_ok=1;objects[1].object.node_matrices.size=0;
 CHECK(!modern_shadows_environment(0,&frame,color,1),"short child pose cannot suppress stock");
 objects[1].object.node_matrices.size=sizeof(real_matrix4x3);
 objects[2].object.first_child_object_index=0;remaining=4;
 CHECK(!environmental_casters(0,&remaining) && remaining==0,"cycles terminate at bounded traversal");
 objects[2].object.first_child_object_index=NONE;mode=2;
 CHECK(modern_shadows_environment(0,&frame,color,1) && soft_mode,"Soft chooses filter without changing ownership");
 cached_bsp=NULL;i=begins;
 CHECK(!modern_shadows_environment(0,&frame,color,1) && begins==i,"map disposal rejects stale geometry");
 return 0;
}
