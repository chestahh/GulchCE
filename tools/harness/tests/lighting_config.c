#include "harness.h"
static unsigned long generation;
static int enabled, reads, change_at;
unsigned long config_changes(void) { return generation; }
int config_boolean(const char *name)
{ reads++;if(reads==change_at){enabled=1;generation++;}return enabled; }
#include "under_test.inc"
int main(int argc,char **argv)
{
 unsigned int i, count = sizeof(preferences) / sizeof(preferences[0]);int before;
 CHECK(!lighting_config_boolean("display.plasma_lighting"),"initial zero generation loads off defaults");
 before=reads;
 for(i=0;i<10000;i++) CHECK(!lighting_config_boolean(preferences[i%count].name),"cached value remains off");
 CHECK(reads==before,"ordinary reads do not reenter general configuration");
 enabled=1;generation++;
 for(i=0;i<count;i++) CHECK(lighting_config_boolean(preferences[i].name),"apply refreshes every related preference");
 CHECK(reads==before+(int)count,"one refresh for a settings generation");
 enabled=0;generation++;
 CHECK(!lighting_config_boolean("display.vehicle_lighting"),"defaults/disable is visible immediately");
 before=reads;CHECK(!lighting_config_boolean("unrelated.setting")&&reads==before+1,"unknown names retain ordinary lookup behavior");
 generation++;change_at=reads+2;
 CHECK(!lighting_config_boolean("display.plasma_lighting"),"first value was read before intervening change");
 CHECK(lighting_config_boolean("display.plasma_lighting"),"intervening change cannot leave a stale value cached");
 return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original OpenCE port: https://github.com/chestahh/GulchCE/tree/035f47bb8fc682675456323dbb3095b506502ec6 */
