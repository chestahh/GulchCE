#include "harness.h"
#include <math.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define REAL_MAX 3.4e38f
#define _object_shadowless_bit 2
typedef struct { float v[16]; } real_matrix4x3;
struct render_distant_light { struct {float red,green,blue;} color; real_vector3d direction; };
struct rasterizer_model_begin_parameters { long unique_identifier; struct {int distant_light_count;struct render_distant_light distant_lights[2];} lighting; };
static struct object_datum { struct {long parent_object_index, first_child_object_index, next_object_index;unsigned flags;} object; } objects[3];
static struct { int rasterizer_target; } global_window_parameters;
static struct { int draw_environment_shadows; } rasterizer_debug_options;
static const void *cached_bsp;
static int mode, begins, valid, enabled, collected, mask_seen, soft_seen;
static float radius=2, direction_seen;
static int setting(void){return mode;}
static struct object_datum *object_try_and_get(long i){return i>=0 && i<3?objects+i:NULL;}
static struct object_datum *object_get(long i){CHECK(i>=0 && i<3,"validated child");return objects+i;}
static void object_get_bounding_sphere(long i,real_point3d *p,real *r){p->x=i;p->y=p->z=0;*r=radius;}
static void object_get_render_bounding_sphere(long i,real_point3d *p,real *r){object_get_bounding_sphere(i,p,r);*r=3;}
static real distance3d(const real_point3d *a,const real_point3d *b){real x=a->x-b->x,y=a->y-b->y,z=a->z-b->z;return sqrtf(x*x+y*y+z*z);}
static void render_interpolation_object_point(long i,real_point3d *p){p->x+=.25f;}
static float normalize3d(real_vector3d *v){float n=sqrtf(v->i*v->i+v->j*v->j+v->k*v->k);if(n){v->i/=n;v->j/=n;v->k/=n;}return n;}
static void perpendicular3d(const real_vector3d *v,real_vector3d *r){r->i=1;r->j=r->k=0;}
static void matrix4x3_from_point_and_vectors(real_matrix4x3 *m,const real_point3d *p,const real_vector3d *r,const real_vector3d *d){memset(m,0,sizeof(*m));m->v[12]=p->x;m->v[10]=d->k;direction_seen=d->k;}
static void matrix_to_gl(const real_matrix4x3 *m,float *p){memcpy(p,m->v,64);}
static void modern_environment_model_clear(void){enabled=0;}
static int modern_environment_model_reuse(void){enabled=valid;return valid;}
static int modern_environment_model_begin(const float *m,float r,int soft){begins++;valid=enabled=0;soft_seen=soft;CHECK(m[12]==.25f,"interpolated family center");CHECK(r==4,"render bounds include attached object beyond collision sphere");return 1;}
static int environmental_casters(long root,int *remaining){CHECK(root==0,"held weapon shares owner family");collected++;return 1;}
static void modern_environment_model_end(int complete,int mask){mask_seen=mask;valid=enabled=complete;}
#include "under_test.inc"
int main(int argc,char **argv){
 struct rasterizer_model_begin_parameters p={0};int before,i;
 for(i=0;i<3;i++)objects[i].object.first_child_object_index=objects[i].object.next_object_index=NONE;
 objects[0].object.first_child_object_index=1;
 objects[0].object.parent_object_index=NONE;objects[1].object.parent_object_index=0;objects[2].object.parent_object_index=2;
 p.lighting.distant_light_count=2;
 p.lighting.distant_lights[0].color.red=1;p.lighting.distant_lights[0].direction.k=-2;
 p.lighting.distant_lights[1].color.blue=1;p.lighting.distant_lights[1].direction.k=1;
 cached_bsp=&p;rasterizer_debug_options.draw_environment_shadows=1;
 modern_shadows_environment_model(&p,0);CHECK(!begins && !enabled,"Off skips GPU setup");
 mode=2;modern_shadows_environment_model(&p,0);CHECK(begins==1 && collected==1 && mask_seen==3 && soft_seen,"both independent distant sources");
 CHECK(direction_seen==1,"direction normalized");
 modern_environment_model_clear();p.unique_identifier=1;
 modern_shadows_environment_model(&p,0);CHECK(begins==1 && enabled,"attached weapon reuses current family maps");
 valid=0;modern_shadows_environment_model(&p,0);CHECK(begins==2,"new window invalidation rebuilds pose");
 modern_shadows_environment_model(&p,1);CHECK(!enabled,"world body does not shade first-person projection");
 before=begins;p.unique_identifier=2;modern_shadows_environment_model(&p,0);CHECK(begins==before && !enabled,"parent cycle bounded");
 p.unique_identifier=0;objects[0].object.flags=1U<<_object_shadowless_bit;
 modern_shadows_environment_model(&p,0);CHECK(!enabled && begins==before,"authored shadowless flag preserved");
 objects[0].object.flags=0;global_window_parameters.rasterizer_target=1;
 modern_shadows_environment_model(&p,0);CHECK(!enabled && begins==before,"secondary targets excluded");
 global_window_parameters.rasterizer_target=0;cached_bsp=NULL;
 modern_shadows_environment_model(&p,0);CHECK(!enabled && begins==before,"unloaded map excludes stale geometry");
 return 0;
}
