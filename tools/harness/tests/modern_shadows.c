#include "harness.h"
static short prepared_sky;
boolean render_shadows = TRUE;
static void sun_prepare(void) {}
static int patches_ready, sun_count;
static void prepare_spots(int mode) { (void)mode; }
static unsigned char *sun_surface_sky;
static int sun_cutout_count;
static void modern_environment_window(void) {}
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SHADOW_LIGHTS 128
#include "budget.inc"
#include "grenade_shadow.inc"
struct structure_bsp { int identity; } map_a, map_b;
static struct structure_bsp *map = &map_a;
static int parts[1], begins, disposals, failed, complete = 1, mode_soft;
static int rebound_stage, failed_slot = -1;
static long rebound_light;
static float captured_radius;
static const char *mode = "off";
static struct { struct { real_point3d position; int mirrored; } camera; short local_player_index; } render;
enum { _director_perspective_first_person, _director_perspective_third_person };
static short perspective = _director_perspective_third_person;
static short director_get_perspective(short player) { return perspective; }
static long player_control_get_unit_index(short player) { return 0x10001; }
static struct { float game_time_sec; } global_frame_parameters;
struct rasterizer_light_submit_parameters {
 real_point3d position; float radius;
 struct { float red, green, blue; } color;
};
static struct { int light_count; struct rasterizer_light_submit_parameters lights[128]; } rasterizer_lights;
/* Dependencies stand in for each category's existing local/host eligibility.
   The dispatcher and shadow selection below are the real production code. */
static unsigned int allowed;
static int shield_glow_selected(long i) { return (i==0 || (i>=7 && i<10)) && (allowed & (1U<<i)); }
static int muzzle_lighting_selected(long i) { return i==1 && (allowed & 2); }
static int covenant_emitter_lighting_selected(long i) { return i==2 && (allowed & 4); }
static int covenant_lighting_selected(long i) { return i==3 && (allowed & 8); }
static int rocket_lighting_selected(long i) { return i==4 && (allowed & 16); }
static int lighting_config_boolean(const char *s) { return (allowed & 0x78)!=0; }
static int authored_emitter_lighting_enabled(void) { return (allowed & 0x60)!=0; }
static int authored_emitter_lighting_category_enabled(int category) { return (allowed & (1U<<category))!=0; }
static struct { long rasterizer_index; int category; } plasma_lighting_attachments[2] = {{5,5},{6,6}};
static int plasma_lighting_count=2;
static int unshadowed_object(long owner,long object) { return owner!=NONE && owner==object; }
static const char *config_string(const char *s) { return mode; }
static struct structure_bsp *global_structure_bsp_get(void) { return map; }
static float distance_squared3d(const real_point3d *a,const real_point3d *b) {
 float x=a->x-b->x,y=a->y-b->y,z=a->z-b->z; return x*x+y*y+z*z;
}
static float plasma_occlusion_specular_scale(long i) { return 2; }
static int collecting_slot;
static long skipped_world[MODERN_SHADOW_LIGHTS], skipped_view[MODERN_SHADOW_LIGHTS], skipped_emitter[MODERN_SHADOW_LIGHTS];
static int modern_shadow_begin(int slot,const float p[3],float radius,int soft, int ground_grenade) {
 collecting_slot=slot; begins++; captured_radius=radius; mode_soft=soft; return !failed && slot!=failed_slot;
}
static void check_static_breakables(struct structure_bsp const *bsp) { (void)bsp; }
static int bsp_changes;
static void modern_shadow_change_bsp(void) { bsp_changes++; }
static void modern_shadow_dispose(void) { disposals++; }
static void modern_shadow_clear_draws(void) {}
static void plasma_falloff_begin(long light) { rebound_stage=2; rebound_light=light; }
static void plasma_falloff_specular_begin(long light) { rebound_stage=1; rebound_light=light; }
static int modern_shadow_end(void) { return !failed; }
static int world_casters(const struct structure_bsp *b,const real_point3d *p,float r) { return complete; }
static long view_owner[MODERN_SHADOW_LIGHTS], world_owner[MODERN_SHADOW_LIGHTS];
static int viewmodel_casters(long owners[2],long stuck_host,long unshadowed_owner) { view_owner[collecting_slot]=unshadowed_owner; skipped_view[collecting_slot]=stuck_host; owners[0]=NONE; owners[1]=0x10001; return complete; }
static int object_casters(const real_point3d *p,float r,const long owners[2],long unshadowed_owner, long excluded_caster, long emitter) { world_owner[collecting_slot]=unshadowed_owner; skipped_emitter[collecting_slot]=emitter; skipped_world[collecting_slot]=excluded_caster; return complete; }
static long chassis_light=NONE, chassis_owner=NONE;
static long muzzle_light=NONE, firing_weapon=NONE;
static long muzzle_lighting_shadow_caster(long light,long emitter_object,long first_person_unit,long *excluded_shooter,long *collision_owner) {
 *excluded_shooter=light==muzzle_light ? first_person_unit : NONE;
 *collision_owner=light==chassis_light ? chassis_owner : NONE;
 return light==muzzle_light ? firing_weapon : *collision_owner;
}
static long exempt_light=NONE, exempt_owner=NONE;
static long stuck_light=NONE, impact_parent=0x10007;
static long plasma_lighting_shadow_owner(long light, struct plasma_shadow_source *grenade) { grenade->stuck_host=light==stuck_light ? impact_parent : NONE; grenade->casing=light==stuck_light ? 27 : NONE; grenade->emitter_object=NONE; grenade->grounded=0; return light==exempt_light ? exempt_owner : NONE; }
static int spot_mask, extra_flashlight;
static int fixtures, device_policy = TRUE;
static long device_index = NONE;
static int light_port_authored_dynamic(long handle) { return handle == device_index; }
static int game_engine_allow_dynamic_lighting(void) { return device_policy; }
static int config_boolean(const char *s) { if (!strcmp(s,"display.environment_fixtures")) return fixtures; CHECK(!strcmp(s,"display.flashlight_extra_slot"),"flashlight budget option"); return extra_flashlight; }
static int modern_spot_mask(void) { return spot_mask; }
static unsigned short mesh_index[1];
#include "under_test.inc"
static int selection_stability(void)
{
 int fps, frame, switches, map_zero, map_one;
 for (fps=30; fps<=120; fps*=2) {
  modern_shadows_reset(); modern_shadows_window();
  modern_shadows_submitted(100,0); modern_shadows_submitted(101,1); modern_shadows_submitted(102,2);
  allowed=7; mode="hard"; modern_shadows_map_loaded(1);
  rasterizer_lights.lights[0].color.red=2;
  rasterizer_lights.lights[1].color.red=1;
  rasterizer_lights.lights[2].color.red=.7f;
  global_frame_parameters.game_time_sec=0;
  sun_count=1;modern_shadows_prepare(1);
  CHECK(!sun_count && !sun_cutout_count,"new view clears previous ownership and map reset clears cutouts");
  map_zero=map_for(0);map_one=map_for(1);switches=0;
  CHECK(map_zero!=NONE && map_one!=NONE && map_zero!=map_one,"two strongest lights receive independent maps");
  for(frame=1;frame<=fps;frame++) {
   global_frame_parameters.game_time_sec=(float)frame/fps;
   rasterizer_lights.lights[2].color.red=frame%2?1.4f:.7f;
   modern_shadows_prepare(1);
   switches+=map_for(0)!=map_zero || map_for(1)!=map_one;
  }
  CHECK(!switches,"brief challengers do not churn either slot");
  for(frame=1;frame<=fps;frame++) {
   global_frame_parameters.game_time_sec=1.f+(float)frame/fps;
   rasterizer_lights.lights[2].color.red=1.4f;modern_shadows_prepare(1);
   if(modern_shadows_selected(2)) break;
  }
  CHECK(frame>=fps/10 && frame<=fps/10+2,"persistent challenger admitted after 100 ms at every cap");
  CHECK(map_for(0)==map_zero && map_for(2)==map_one && !modern_shadows_selected(1),"replace weaker light without moving surviving map");
  modern_shadows_model_slot(2,0);modern_shadows_model_slot(0,1);
  CHECK(modern_shadows_model_mask()==3 && modern_shadows_map(0)==map_one && modern_shadows_map(1)==map_zero,"both model slots resolve independently of shadow slot order");
  modern_shadows_world_begin(2,2);
  CHECK(modern_shadows_map(0)==map_one && modern_shadows_map(1)==NONE,"world draw binds just its own light");
  modern_shadows_world_end();
  rasterizer_lights.lights[1].color.red=4;modern_shadows_prepare(1);
  CHECK(modern_shadows_selected(1) && map_for(0)==map_zero,"dominant flash responds immediately without evicting both lights");
  allowed=4;modern_shadows_prepare(1);
  CHECK(!modern_shadows_selected(0) && !modern_shadows_selected(1) && modern_shadows_selected(2),"host rejection is immediate for either slot");
  allowed=7;rasterizer_lights.lights[0].color.red=2;rasterizer_lights.lights[1].color.red=.7f;
  modern_shadows_prepare(1);
  rasterizer_lights.lights[1].color.red=1.8f;modern_shadows_prepare(1);
  global_frame_parameters.game_time_sec=0;modern_shadows_prepare(1);
  CHECK(modern_shadows_selected(2) && !modern_shadows_selected(1),"clock rewind restarts challenger interval");
  modern_shadows_submitted(999,2);modern_shadows_prepare(1);
  CHECK(modern_shadows_selected(1) && !modern_shadows_selected(2),"recycled identity cannot keep its old slot");
  rasterizer_lights.lights[0].color.red=rasterizer_lights.lights[1].color.red=rasterizer_lights.lights[2].color.red=0;
  modern_shadows_prepare(1);CHECK(!active,"zero-intensity lights do not retain maps");
 }
 return 0;
}
static int full_budget(void)
{
 int i,j,original[8];
 modern_shadows_reset(); modern_shadows_window();
 rasterizer_lights.light_count=10; allowed=1023; mode="hard"; modern_shadows_map_loaded(1);
 for(i=0;i<10;i++) {
  rasterizer_lights.lights[i].radius=2;
  rasterizer_lights.lights[i].color.red=i<8?2.f:1.f;
  modern_shadows_submitted(100+i,i);
 }
 modern_shadows_prepare(1);
 CHECK(active==255,"eight eligible lights receive complete maps");
 spot_mask=7;modern_shadows_prepare(1);
 CHECK(__builtin_popcount(active)==7,"one published flashlight shares a slot even with two fixtures");
 extra_flashlight=1;modern_shadows_prepare(1);
 CHECK(active==255,"extra retains eight other lights with an active projector");
 extra_flashlight=0;spot_mask=3;modern_shadows_prepare(1);
 CHECK(active==255,"fixtures alone and failed or faded beams leave every slot available");
 for(i=0;i<8;i++) {
  original[i]=map_for(i); CHECK(original[i]!=NONE,"all eight lights are covered");
  for(j=0;j<i;j++) CHECK(original[i]!=original[j],"no light shares another light's map");
 }
 CHECK(!modern_shadows_selected(8) && !modern_shadows_selected(9),"over-budget lights retain ordinary lighting");
 modern_shadows_model_slot(7,0);modern_shadows_model_slot(6,1);
 CHECK(modern_shadows_model_mask()==3 && modern_shadows_map(0)==original[7] &&
  modern_shadows_map(1)==original[6],"receivers can bind the final two maps");
 failed_slot=original[7];modern_shadows_prepare(1);
 CHECK(!modern_shadows_selected(7) && modern_shadows_selected(6),"failure in the eighth map does not invalidate seven others");
 failed_slot=-1;modern_shadows_prepare(1);
 allowed&=~(1U<<3);modern_shadows_prepare(1);
 CHECK(!modern_shadows_selected(3) && modern_shadows_selected(8),"policy rejection releases its slot immediately");
 CHECK(map_for(8)==original[3],"vacancy reused without reshuffling surviving maps");
 for(i=0;i<8;i++) if(i!=3) CHECK(map_for(i)==original[i],"surviving maps remain stable");
 rasterizer_lights.lights[9].color.red=20;modern_shadows_prepare(1);
 CHECK(modern_shadows_selected(9) && !modern_shadows_selected(8),"dominant flash replaces weakest incumbent");
 allowed=0;modern_shadows_prepare(1);
 CHECK(!active,"host rejection clears every slot");
 return 0;
}
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
 modern_shadows_prepare(1);
 CHECK(!begins && !active, "pending setting cannot change a running map");
 modern_shadows_map_loaded(1);
 for(i=0;i<7;i++) {
  allowed=1U<<i;
  modern_shadows_prepare(1);
  CHECK(active && modern_shadows_selected(i), "every eligible light category can supply the shadow map");
  CHECK(captured_radius==4, "specular extent is covered");
  modern_shadows_model_slot(i,0); modern_shadows_model_slot(NONE,1);
  CHECK(modern_shadows_model_mask()==1 && modern_shadows_selected(i), "only the selected light slot is shadowed");
  allowed=0; modern_shadows_prepare(1);
  CHECK(!active, "disabled or policy-rejected categories cannot keep a map");
 }
 device_index=100; fixtures=1; modern_shadows_prepare(1);
 CHECK(modern_shadows_selected(0),"a device attachment can shadow with no weapon lighting category");
 CHECK(skipped_world[map_for(0)]==NONE,"a device light includes ordinary world casters");
 modern_shadows_model_slot(0,0);modern_shadows_receiver(0,0x10003);
 CHECK(modern_shadows_model_mask()==1,"a native device light shadows ordinary model receivers");
 device_policy=0;modern_shadows_prepare(1);CHECK(!active,"host policy excludes device shadows");
 device_policy=1;fixtures=0;modern_shadows_prepare(1);CHECK(!active,"disabled fixtures release device shadows");
 device_index=NONE;
 allowed=3;stuck_light=100;
 for(i=0;i<2;i++) {
  mode=i ? "soft" : "hard"; modern_shadows_map_loaded(1); modern_shadows_prepare(1);
  modern_shadows_model_slot(0,0);modern_shadows_model_slot(1,1);
  modern_shadows_receiver(0,impact_parent);
  CHECK(modern_shadows_model_mask()==3 && modern_shadows_collision_owner(0)==NONE,
        "stuck host receives both maps with no chassis override");
  CHECK(skipped_world[map_for(0)]==impact_parent && skipped_view[map_for(0)]==impact_parent &&
        skipped_world[map_for(1)]==NONE && skipped_view[map_for(1)]==NONE &&
        skipped_emitter[map_for(0)]==27 && skipped_emitter[map_for(1)]==NONE,
        "only the stuck grenade omits its impact parent in world and first person");
 }
 mode="hard"; modern_shadows_map_loaded(1);stuck_light=NONE;modern_shadows_prepare(1);
 CHECK(skipped_world[map_for(0)]==NONE && skipped_view[map_for(0)]==NONE,"detachment restores host casting");
 allowed=3;muzzle_light=101;firing_weapon=0x10009;modern_shadows_prepare(1);
 CHECK(skipped_world[map_for(1)]==firing_weapon && skipped_view[map_for(1)]==firing_weapon &&
       skipped_world[map_for(0)]==NONE && modern_shadows_collision_owner(1)==NONE,
       "only the handheld flash omits its firing weapon, without chassis blocking");
 modern_shadows_model_slot(1,0);modern_shadows_receiver(0,firing_weapon);
 CHECK(modern_shadows_map(0)!=NONE,"the firing weapon samples shadows from the biped and other geometry");
 perspective=_director_perspective_first_person;modern_shadows_prepare(1);
 CHECK(view_owner[map_for(1)]==0x10001 && world_owner[map_for(1)]==0x10001 && world_owner[map_for(0)]==NONE,
       "only the viewer's flash omits their arms and world body");
 modern_shadows_receiver(1,NONE);
 CHECK(modern_shadows_map(0)!=NONE,"other blockers can still shadow the viewer's hands");
 perspective=_director_perspective_third_person;modern_shadows_prepare(1);
 CHECK(view_owner[map_for(1)]==NONE && world_owner[map_for(1)]==NONE,"switching views restores the shooter's silhouette");
 muzzle_light=NONE;chassis_light=101;chassis_owner=0x10002;modern_shadows_prepare(1);
 CHECK(modern_shadows_collision_owner(1)==chassis_owner && modern_shadows_collision_owner(0)==NONE,
       "collision supplement belongs only to its own muzzle light");
 modern_shadows_model_slot(1,0);modern_shadows_receiver(0,chassis_owner);
 CHECK(modern_shadows_map(0)!=NONE,"chassis still receives shadows from other casters in its muzzle map");
 complete=0;modern_shadows_prepare(1);
 CHECK(modern_shadows_collision_owner(1)==NONE,"failed map returns to full collision visibility");
 complete=1;chassis_light=NONE;modern_shadows_prepare(1);
 CHECK(modern_shadows_collision_owner(1)==NONE,"removed pulse cannot leave collision ownership");
 allowed=3; exempt_light=100; exempt_owner=0x10001; modern_shadows_prepare(1);
 modern_shadows_model_slot(0,0);modern_shadows_model_slot(1,1);
 modern_shadows_receiver(0,exempt_owner);
 CHECK(modern_shadows_model_mask()==2 && modern_shadows_map(0)==NONE && modern_shadows_map(1)!=NONE,
       "own light is unshadowed while external light still shadows the owner");
 CHECK(modern_shadows_selected(0),"owner exemption must not enable collision-mask fallback");
 modern_shadows_world_begin(0,2);
 CHECK(modern_shadows_map(0)!=NONE,"own light still shadows the world");modern_shadows_world_end();
 modern_shadows_receiver(0,0x20001);
 CHECK(modern_shadows_model_mask()==3,"another or recycled receiver retains both shadow maps");
 modern_shadows_receiver(1,NONE);
 CHECK(modern_shadows_model_mask()==2,"view arms and weapon use the current wearer for their own shield only");
 modern_shadows_receiver(0,NONE);
 CHECK(modern_shadows_model_mask()==3,"receiver end clears exception");
 exempt_light=NONE;modern_shadows_prepare(1);modern_shadows_receiver(0,exempt_owner);
 CHECK(modern_shadows_model_mask()==3,"removed exception cannot survive map preparation");
 modern_shadows_receiver(0,NONE);
 CHECK(modern_shadows_selected(0) && modern_shadows_selected(1), "initial pair is deterministic");
 failed_slot=1;modern_shadows_prepare(1);
 CHECK(modern_shadows_selected(0) && !modern_shadows_selected(1), "one failed map preserves the other and cannot reuse old depth");
 failed_slot=-1;modern_shadows_prepare(1);
 rasterizer_lights.lights[1].color.red=1.1f;
 modern_shadows_prepare(1);
 CHECK(map_for(0)==0 && map_for(1)==1, "near-equal scores do not reshuffle maps");
 modern_shadows_submitted(200,0); rasterizer_lights.lights[1].color.red=2;
 mode="soft"; modern_shadows_map_loaded(1); modern_shadows_prepare(1);
 CHECK(modern_shadows_selected(1) && mode_soft, "new identity and stronger light are reconsidered");
 modern_shadows_receiver(1, NONE);
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
 before=bsp_changes; map=&map_b; modern_shadows_prepare(1);
 CHECK(bsp_changes==before+1, "BSP transition releases old meshes");
 CHECK(active, "BSP transition retries receiver resources");
 modern_shadows_world_begin(1,1); modern_shadows_cancel();
 CHECK(rebound_stage==1 && rebound_light==1, "failed specular shader restores its collision mask");
 prepared_sky=1; patches_ready=1;
 before=disposals; mode="off"; modern_shadows_map_loaded(1); modern_shadows_prepare(1);
 CHECK(prepared_sky==NONE && !patches_ready,"disable clears the prepared map sources");
 CHECK(disposals==before+1 && !cached_bsp && !active, "disable releases resources");
 modern_shadows_window();
 CHECK(identities[0]==NONE && slots[0]==NONE, "window transition clears light identities and slots");
 CHECK(!modern_shadows_view_receiver() && receiver_object==NONE, "window transition clears receiver kind and identity");
 return MODERN_SHADOW_LIGHTS==2 ? selection_stability() : full_budget();
}
