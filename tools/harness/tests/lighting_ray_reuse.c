#include "harness.h"
enum { PLASMA_COLLISION_RAYS=2, MAXIMUM_BREAKABLE_SURFACES_PER_MAP=16 };
struct collision_bsp { int index; };
struct collision_bsp_test_vector_result { int leaf_count, leaf_indices[2]; float t; };
static struct { struct collision_bsp *bsp; int ray; } batch;
static byte flags[2];
static byte *breakable_surface_flags_get(void) { return flags; }
#include "survey.inc"

int main(void) {
    real_point3d p={1,2,3}; real_vector3d v={2,3,4};
    struct collision_bsp bsp;
    struct collision_bsp_test_vector_result hit={1,{3},.75f};
    float depth=.75f;
    batch.bsp=&bsp; batch.ray=1;
    reuse_survey_begin(); reuse_survey_world(&p,&v,1,&hit); reuse_survey_end(&depth,1);
    depth=.25f;
    reuse_survey_begin(); reuse_survey_world(&p,&v,1,&hit); reuse_survey_end(&depth,1);
    CHECK(survey_repeats==1 && survey_finals==1 && survey_object_changes==1 && !survey_world_changes,
          "moving object changes final depth independently of world");
    hit.leaf_indices[0]=7;
    reuse_survey_begin(); reuse_survey_world(&p,&v,1,&hit); reuse_survey_end(&depth,1);
    CHECK(survey_world_changes==1 && survey_object_changes==1, "changed traversal is not an object change");
    flags[0]=1;
    reuse_survey_begin(); reuse_survey_world(&p,&v,1,&hit); reuse_survey_end(&depth,1);
    CHECK(survey_repeats==2 && survey_rays==4, "breakable change invalidates world reuse");
    return 0;
}
