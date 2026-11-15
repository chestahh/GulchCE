#include "harness.h"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SHADOW_LIGHTS 128
struct structure_bsp { int identity; } map_a, map_b;
static struct structure_bsp *map = &map_a;
static int parts[1], begins, disposals, failed, complete = 1, mode_soft;
static int rebound_stage;
static long rebound_light;
static float captured_radius;
static const char *mode = "off";
static struct { struct { real_point3d position; } camera; } render;
struct rasterizer_light_submit_parameters {
 real_point3d position; float radius;
 struct { float red, green, blue; } color;
};
static struct { int light_count; struct rasterizer_light_submit_parameters lights[128]; } rasterizer_lights;
/* Dependencies stand in for each category's existing local/host eligibility.
   The dispatcher and shadow selection below are the real production code. */
static unsigned int allowed;
static int shield_glow_selected(long i) { return i==0 && (allowed & 1); }
static int muzzle_lighting_selected(long i) { return i==1 && (allowed & 2); }
static int covenant_emitter_lighting_selected(long i) { return i==2 && (allowed & 4); }
static int covenant_lighting_selected(long i) { return i==3 && (allowed & 8); }
static int rocket_lighting_selected(long i) { return i==4 && (allowed & 16); }
static int lighting_config_boolean(const char *s) { return (allowed & 0x78)!=0; }
static int authored_emitter_lighting_enabled(void) { return (allowed & 0x60)!=0; }
static int authored_emitter_lighting_category_enabled(int category) { return (allowed & (1U<<category))!=0; }
static struct { long rasterizer_index; int category; } plasma_lighting_attachments[2] = {{5,5},{6,6}};
static int plasma_lighting_count=2;
static const char *config_string(const char *s) { return mode; }
static struct structure_bsp *global_structure_bsp_get(void) { return map; }
static float distance_squared3d(const real_point3d *a,const real_point3d *b) {
 float x=a->x-b->x,y=a->y-b->y,z=a->z-b->z; return x*x+y*y+z*z;
}
static float plasma_occlusion_specular_scale(long i) { return 2; }
static int modern_shadow_begin(const float p[3],float radius,int soft) {
 begins++; captured_radius=radius; mode_soft=soft; return !failed;
}
static void modern_shadow_dispose(void) { disposals++; }
static void modern_shadow_clear_draws(void) {}
static void plasma_falloff_begin(long light) { rebound_stage=2; rebound_light=light; }
static void plasma_falloff_specular_begin(long light) { rebound_stage=1; rebound_light=light; }
static int modern_shadow_end(void) { return !failed; }
static int world_casters(const struct structure_bsp *b,const real_point3d *p,float r) { return complete; }
static int viewmodel_casters(long owners[2]) { owners[0]=owners[1]=NONE; return complete; }
static int object_casters(const real_point3d *p,float r,const long owners[2]) { return complete; }
#include "under_test.inc"
int main(int argc,char **argv)
{
 int i, before;
 rasterizer_lights.light_count=7;
 modern_shadows_window();
 for(i=0;i<7;i++) {
  rasterizer_lights.lights[i].radius=2;
  rasterizer_lights.lights[i].color.red=1;
  modern_shadows_submitted(100+i,i);
 }
 allowed=127;
 modern_shadows_prepare(1);
 CHECK(!begins && !active, "off performs no depth setup");
 mode="hard";
 for(i=0;i<7;i++) {
  allowed=1U<<i;
  modern_shadows_prepare(1);
  CHECK(active && selected_rasterizer==i, "every eligible light category can supply the shadow map");
  CHECK(captured_radius==4, "specular extent is covered");
  modern_shadows_model_slot(i,0); modern_shadows_model_slot(NONE,1);
  CHECK(modern_shadows_model_mask()==1 && modern_shadows_selected(i), "only the selected light slot is shadowed");
  allowed=0; modern_shadows_prepare(1);
  CHECK(!active, "disabled or policy-rejected categories cannot keep a map");
 }
 allowed=3; modern_shadows_prepare(1);
 CHECK(selected_identity==100, "initial tie is deterministic");
 rasterizer_lights.lights[1].color.red=1.1f;
 modern_shadows_prepare(1);
 CHECK(selected_identity==100, "near-equal scores do not churn identity");
 modern_shadows_submitted(200,0); rasterizer_lights.lights[1].color.red=2;
 mode="soft"; modern_shadows_prepare(1);
 CHECK(selected_identity==101 && mode_soft, "new identity and stronger light are reconsidered");
 modern_shadows_receiver(1);
 CHECK(modern_shadows_view_receiver(), "first-person receiver selects its depth map");
 modern_shadows_world_begin(1,1);
 CHECK(!modern_shadows_view_receiver(), "world illumination cannot sample viewmodel depth");
 CHECK(modern_shadows_world_stage()==1 && modern_shadows_world_extent()==8, "specular coordinates retain their extent");
 complete=0; modern_shadows_prepare(1);
 CHECK(!active && !modern_shadows_selected(1), "incomplete casters retain collision visibility");
 complete=1; modern_shadows_prepare(0);
 CHECK(!active, "secondary targets cannot reuse primary shadows");
 modern_shadows_prepare(1); modern_shadows_world_begin(1,2); modern_shadows_cancel();
 CHECK(!active && !modern_shadows_model_mask(), "shader failure restores ordinary visibility");
 CHECK(rebound_stage==2 && rebound_light==1, "failed world shader restores the already-bound collision mask");
 before=begins; modern_shadows_prepare(1);
 CHECK(begins==before, "failed receiver stops unused depth passes until reset");
 before=disposals; map=&map_b; modern_shadows_prepare(1);
 CHECK(disposals==before+1, "BSP transition releases old meshes");
 CHECK(active, "BSP transition retries receiver resources");
 modern_shadows_world_begin(1,1); modern_shadows_cancel();
 CHECK(rebound_stage==1 && rebound_light==1, "failed specular shader restores its collision mask");
 before=disposals; mode="off"; modern_shadows_prepare(1);
 CHECK(disposals==before+1 && !cached_bsp && !active, "disable releases resources");
 modern_shadows_window();
 CHECK(identities[0]==NONE && slots[0]==NONE, "window transition clears light identities and slots");
 CHECK(!modern_shadows_view_receiver(), "window transition clears receiver kind");
 return 0;
}
