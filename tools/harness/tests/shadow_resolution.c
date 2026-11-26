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
 for(size=128;size<=2048;size*=2) {
  selected=size;CHECK(shadow_scale_choose()==size/128,"every menu resolution is exact");
  selected=size*2-1;CHECK(shadow_scale_choose()==size/128,"intermediate values round down");
 }
 selected=4096;CHECK(shadow_scale_choose()==16,"retired resolution clamps to 2048");
 selected=LONG_MAX;CHECK(shadow_scale_choose()==16,"oversized input clamps without overflow");
 return 0;
}
