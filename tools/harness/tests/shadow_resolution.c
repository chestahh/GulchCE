#include "harness.h"
#include <limits.h>
static long selected;
static long config_integer(char const *name) {
 CHECK(!strcmp(name,"display.shadow_resolution"),"reuse the established setting");
 return selected;
}
#include "under_test.inc"
int main(int argc,char **argv) {
 long size;
 selected=LONG_MIN;CHECK(shadow_scale_choose()==1,"negative input remains stock");
 selected=0;CHECK(shadow_scale_choose()==1,"zero input remains stock");
 for(size=128;size<=4096;size*=2) {
  selected=size;CHECK(shadow_scale_choose()==size/128,"every menu resolution is exact");
  selected=size*2-1;CHECK(shadow_scale_choose()==size/128,"intermediate values round down");
 }
 selected=LONG_MAX;CHECK(shadow_scale_choose()==32,"oversized input clamps without overflow");
 return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
