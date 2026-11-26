#include "harness.h"
#include <math.h>

static float spot_planes[6][4];
#include "under_test.inc"

int main(void)
{
    /* Forward +Z, 90 degree cone, reverse depth from one to ten. */
    const float frame[16] = {1,0,0,0, 0,1,0,0, 0,0,-1.f/9,1, 0,0,10.f/9,0};
    const real_point3d outside[] = {
        {-5,0,4},{5,0,4},{0,-5,4},{0,5,4},{0,0,.5f},{0,0,11}
    };
    real_point3d edge = {4.2f,0,4};
    float low[3]={3.8f,-.1f,3.9f}, high[3]={4.6f,.1f,4.1f};
    spot_clip_planes(frame);
    for (unsigned i=0; i<sizeof(outside)/sizeof(*outside); ++i) {
        float p[3]={outside[i].x,outside[i].y,outside[i].z};
        CHECK(!spot_sphere_visible(&outside[i],.1f),"sphere outside beam");
        CHECK(!spot_box_visible(p,p),"batch outside beam");
    }
    CHECK(spot_sphere_visible(&edge,.2f),"partly visible sphere");
    CHECK(spot_box_visible(low,high),"partly visible batch");
    return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
