#include "harness.h"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define VALID_INDEX(i,n) ((i)>=0 && (i)<(n))
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&((t *)(b)->address)[i])
#define MAXIMUM_REGIONS_PER_OBJECT 8
#define _model_detail_rules_outside 1
#define MODERN_SHADOW_VIEW 2
typedef unsigned short word;
typedef struct { float n[4]; } real_vector4d;
typedef struct real_matrix4x3 { float offset; } real_matrix4x3;
struct tag_block { int count;void *address; };
struct tag_reference { long index; };
struct vertex_buffer { int unused; };
struct triangle_buffer { int count; };
struct shader { struct { int type; } base; };
struct shader_texture_animation { int marker; };
struct render_animation { void *colors;const float *values; };
struct model_node { real_matrix4x3 runtime_default_inverse_matrix; } node_data[2];
struct model_region_permutation { int geometry; } permutation;
struct model_region { struct tag_block permutations; } region;
struct model { struct tag_block nodes,regions,geometries,shaders; struct {float i,j;} base_map_scale; } model;
static struct { float game_time_sec; } global_frame_parameters;
struct bitmap_group { struct tag_block bitmaps; } bitmap_group;
struct bitmap_data { int type; } bitmap;
static int bitmap_slot,transforms,submissions,submitted_views,submitted_mesh;
static float submitted_uv[8],submitted_bones[32];
static struct { int mesh; } parts[1]={{17}};
static void *shader_ptr;
static struct shader *shader_definition_get(long i) { return shader_ptr; }
static struct bitmap_group *bitmap_group_get(long i) { return &bitmap_group; }
static struct bitmap_data *bitmap_group_try_and_get_bitmap(long i,int slot) { bitmap_slot=slot;return &bitmap; }
static void *_texture_cache_bitmap_get_hardware_format(struct bitmap_data *b,int a,int c) { return b; }
static void shader_texture_animation_evaluate(const struct shader_texture_animation *a,const struct render_animation *r,
 float u,float v,float x,float y,float z,float time,real_vector4d *ut,real_vector4d *vt) {
 CHECK(a->marker==77 && r && r->values[0]==.25f && time==3,"authored animation uses current object functions and render time");
 transforms++;ut->n[0]=0;ut->n[1]=-v;ut->n[2]=0;ut->n[3]=r->values[0];
 vt->n[0]=u;vt->n[1]=0;vt->n[2]=0;vt->n[3]=time;
}
static void shader_environment_texture_animation_evaluate(const struct shader *s,float t,float *u,float *v) { *u=t;*v=-t; }
static void matrix4x3_multiply(const real_matrix4x3 *a,const real_matrix4x3 *b,real_matrix4x3 *out) { out->offset=a->offset+b->offset; }
static void matrix_to_gl(const real_matrix4x3 *in,float *out) { memset(out,0,64);out[12]=in->offset; }
static int test_level, test_rules;
static void render_object_model_detail(long i, const struct model *m, short *l, short *r) { CHECK(i==7,"beam caster datum");*l=2;*r=0; }
static int model_detail_geometry_index(const struct model *m,const struct model_region_permutation *p,int l,int r) { test_level=l;test_rules=r;return p->geometry; }
static int mesh_get(const struct vertex_buffer *v,const void *key,const struct triangle_buffer *t,const word *w,int c,int n) { return 0; }
static int modern_shadow_draw(int mesh,const float *matrices,int nodes,const void *alpha,const float uv[8],int views) {
 submissions++;submitted_views=views;submitted_mesh=mesh;memcpy(submitted_uv,uv,32);memcpy(submitted_bones,matrices,nodes*64);return 1;
}
#include "under_test.inc"
int main(int argc,char **argv) {
 struct shadow_model_shader shader;struct shadow_environment_shader environment;
 struct shadow_geometry geometry;struct shadow_part part;struct shadow_shader_reference reference;
 real_matrix4x3 pose[2]={{10},{20}};float values[4]={.25f},scale[2]={2,3},uv[8];void const *alpha;
 byte selected[1]={0};int before;
 memset(&shader,0,sizeof(shader));memset(&part,0,sizeof(part));memset(&reference,0,sizeof(reference));
 shader.base.base.type=4;shader.base_map.index=3;shader.u_scale=4;shader.v_scale=5;shader.animation.marker=77;
 shader_ptr=&shader;bitmap_group.bitmaps.count=4;global_frame_parameters.game_time_sec=3;
 CHECK(material(&shader.base,2,&alpha,scale,values,uv)==1 && alpha==&bitmap && bitmap_slot==2,"cutout selects authored bitmap permutation");
 CHECK(uv[1]==-15 && uv[4]==8 && uv[3]==.25f && uv[7]==3,"cutout retains UV rotation, scale and animation");
 shader.flags=8;before=transforms;
 CHECK(!material(&shader.base,0,&alpha,scale,values,uv) && transforms==before,"alpha-blended decal never becomes an opaque caster");
 shader.base.base.type=7;
 CHECK(!material(&shader.base,0,&alpha,scale,values,uv),"transparent effects are not solid blockers");
 shader.base.base.type=4;shader.flags=4;
 CHECK(material(&shader.base,0,&alpha,scale,values,uv)==1 && !alpha && transforms==before,"opaque materials skip unnecessary alpha evaluation");
 memset(&environment,0,sizeof(environment));environment.base.base.type=3;environment.flags=1;environment.base_map.index=3;
 CHECK(material(&environment.base,0,&alpha,scale,NULL,uv)==1 && uv[3]==3 && uv[7]==-3,"environment cutout follows its scroll animation");
 shader.flags=0;
 node_data[0].runtime_default_inverse_matrix.offset=-1;node_data[1].runtime_default_inverse_matrix.offset=-2;
 model.nodes.count=2;model.nodes.address=node_data;model.regions.count=1;model.regions.address=&region;
 region.permutations.count=1;region.permutations.address=&permutation;permutation.geometry=0;
 model.geometries.count=1;model.geometries.address=&geometry;geometry.parts.count=1;geometry.parts.address=&part;
 model.shaders.count=1;model.shaders.address=&reference;reference.shader.index=1;reference.permutation=1;
 model.base_map_scale.i=2;model.base_map_scale.j=3;part.triangle_buffer.count=1;
 CHECK(model_caster(&model,pose,selected,3,values,MODERN_SHADOW_VIEW,7),"animated model collected");
 CHECK(test_level==2 && test_rules==0,"beam depth uses the visible permutation geometry");
 CHECK(submitted_bones[12]==9 && submitted_bones[28]==18,"each animated bone uses the original inverse bind pose");
 CHECK(bitmap_slot==3 && submitted_mesh==17 && submitted_views==MODERN_SHADOW_VIEW && submitted_uv[7]==3,"forced material permutation and receiver mask survive submission");
 before=submissions;selected[0]=255;
 CHECK(model_caster(&model,pose,selected,0,values,3,NONE) && submissions==before,"removed/damaged region NONE is not an invalid unsigned index");
 selected[0]=1;
 CHECK(!model_caster(&model,pose,selected,0,values,3,NONE),"invalid region falls back instead of an incomplete shadow");
 CHECK(model_caster(&model,pose,NULL,0,values,2,NONE),"viewmodel selects the visible default region permutation");
 return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
