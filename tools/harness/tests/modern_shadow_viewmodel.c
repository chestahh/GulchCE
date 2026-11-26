#include "harness.h"
#define MAXIMUM_NODES_PER_ANIMATION 64
#define VALID_INDEX(i,n) ((i)>=0 && (i)<(n))
#define TAG_BLOCK_GET_ELEMENT(b,i,t) (&((t *)(b)->address)[i])
typedef struct real_matrix4x3 { float marker; } real_matrix4x3;
struct tag_block { int count; void *address; };
struct model { struct tag_block nodes; } model;
struct animation_graph { struct tag_block nodes; } graph;
struct reference { long index; };
struct game_globals_first_person_interface { struct reference hands; } interface;
static struct { struct tag_block first_person_interface; } globals;
struct weapon_definition { struct { struct { struct reference first_person_model, first_person_animations; } interface_definition; } weapon; } definition;
static struct { long definition_index; } weapon;
static struct { long unit_index; } player;
struct first_person_weapon {
 int visible, unit_index, weapon_index, weapon_node_remapping_table_valid, hands_node_remapping_table_valid;
 short weapon_node_remapping_table[64], hands_node_remapping_table[64];
 real_matrix4x3 node_matrices[64];
} first;
static struct { int local_player_index; struct { int mirrored; } camera; } render;
static int visible=1, player_index=1, copies;
static int viewmodel_is_visible(void) { return visible; }
static long local_player_get_player_index(int i) { return player_index; }
/* Typed wrappers match the owning translation unit's return types. */
#define player_get(i) (&player)
#define weapon_get(i) (&weapon)
#define scenario_get_game_globals() (&globals)
static struct first_person_weapon *first_person_weapon_get(int i) { return &first; }
static struct weapon_definition *weapon_definition_get(long i) { return &definition; }
static struct model *model_definition_get(long i) { return &model; }
static struct animation_graph *animation_graph_definition_get(long i) { return &graph; }
static void model_remap_node_matrices_to_match_animation_graph(long m,real_matrix4x3 *out,long a,
 const real_matrix4x3 *in,const short *remap) {
 int n; copies++;for(n=0;n<model.nodes.count;n++) out[n]=in[remap[n]];
}
enum { NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS = 1, MODERN_SHADOW_VIEW = 2 };
static int hand_draws, weapon_draws;
static void object_function_values_for_render(long owner, real *values) { values[0] = (real)owner; }
static int model_caster(struct model const *m, real_matrix4x3 const *nodes, const void *regions,
 int permutation, real const *values, int views, long flashlight_object) {
 if (values[0] == first.unit_index) hand_draws++; else if (values[0] == first.weapon_index) weapon_draws++;
 return 1;
}
static int unshadowed_object(long owner,long object) {
 return owner==first.unit_index && (object==first.unit_index || object==first.weapon_index);
}
#include "under_test.inc"
int main(int argc,char **argv) {
 real_matrix4x3 nodes[64]; long index,owner; int n;
 player.unit_index=first.unit_index=10; first.weapon_index=20;
 first.visible=first.weapon_node_remapping_table_valid=first.hands_node_remapping_table_valid=1;
 definition.weapon.interface_definition.first_person_model.index=30;
 definition.weapon.interface_definition.first_person_animations.index=40;
 globals.first_person_interface.count=1;globals.first_person_interface.address=&interface;interface.hands.index=50;
 model.nodes.count=graph.nodes.count=2;
 first.node_matrices[0].marker=1;first.node_matrices[1].marker=2;
 first.weapon_node_remapping_table[0]=1;first.weapon_node_remapping_table[1]=0;
 first.hands_node_remapping_table[0]=0;first.hands_node_remapping_table[1]=1;
 CHECK(modern_shadows_viewmodel(0,&index,&owner,nodes)==1 && index==30 && owner==20 && nodes[0].marker==2,
       "weapon uses its visible interpolated remapping and owner");
 CHECK(modern_shadows_viewmodel(1,&index,&owner,nodes)==1 && index==50 && owner==10 && nodes[0].marker==1,
       "hands use their own remapping and unit owner");
 first.node_matrices[0].marker=7;
 CHECK(modern_shadows_viewmodel(1,&index,&owner,nodes)==1 && nodes[0].marker==7,"current frame pose, not a cached tick");
 first.weapon_index=21;definition.weapon.interface_definition.first_person_model.index=31;
 first.weapon_node_remapping_table[0]=0;
 CHECK(modern_shadows_viewmodel(0,&index,&owner,nodes)==1 && index==31 && owner==21 && nodes[0].marker==7,
       "weapon changes select the new model, owner and pose without an AR-specific cache");
 {
  long owners[2];
  CHECK(viewmodel_casters(owners,first.unit_index,NONE) && !hand_draws && weapon_draws==1 &&
        owners[0]==first.weapon_index && owners[1]==first.unit_index,"stuck player keeps the held weapon casting");
  CHECK(viewmodel_casters(owners,first.weapon_index,NONE) && hand_draws==1 && weapon_draws==1,
        "stuck weapon keeps the player's hands casting");
  CHECK(viewmodel_casters(owners,NONE,NONE) && hand_draws==2 && weapon_draws==2,"external light receives both draws");
  CHECK(viewmodel_casters(owners,NONE,first.unit_index) && hand_draws==2 && weapon_draws==2,
        "wearer and equipped view weapon leave both shield depth layers clear");
 }
 n=copies;visible=0;
 CHECK(modern_shadows_viewmodel(0,&index,&owner,nodes)==0 && copies==n,"hidden/third-person view has no viewmodel casters");
 visible=1;first.visible=0;
 CHECK(!modern_shadows_viewmodel(0,&index,&owner,nodes),"zoom/unequipped state has no casters");
 first.visible=1;player.unit_index=11;
 CHECK(!modern_shadows_viewmodel(0,&index,&owner,nodes),"respawn cannot reuse previous unit's pose");
 player.unit_index=10;render.camera.mirrored=1;
 CHECK(!modern_shadows_viewmodel(0,&index,&owner,nodes),"mirror does not submit viewmodels");
 render.camera.mirrored=0;first.hands_node_remapping_table[0]=64;
 CHECK(modern_shadows_viewmodel(1,&index,&owner,nodes)==-1,"invalid remapping falls back without reading outside pose");
 first.hands_node_remapping_table[0]=0;model.nodes.count=65;
 CHECK(modern_shadows_viewmodel(1,&index,&owner,nodes)==-1,"oversized models cannot overwrite collector's buffer");
 model.nodes.count=2;interface.hands.index=NONE;
 CHECK(modern_shadows_viewmodel(1,&index,&owner,nodes)==0 && owner==10,"missing hands still exclude the hidden body from weapon receiving");
 return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
