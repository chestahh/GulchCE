#include "harness.h"
#include <string.h>
static int values[7], writes;
static char shadow_mode[16]="off";
static const char *keys[] = {"display.weapon_glow", "display.fuel_rod_lighting",
	"display.ammo_counter_lighting", "display.needler_lighting",
	"display.jackal_shield_lighting", "display.covenant_shield_lighting", "display.vehicle_lighting"};
int config_boolean(const char *name)
{
	int i;
	for (i = 0; i < 7; i++) if (!strcmp(keys[i], name)) return values[i];
	return 0;
}
int config_write(const char *name, const char *value)
{
	int i;
	if (!strcmp(name,"display.modern_shadows")) { snprintf(shadow_mode,sizeof(shadow_mode),"%s",value);writes++;return 1; }
	for (i = 0; i < 7; i++) if (!strcmp(keys[i], name))
	{
		values[i] = !strcmp(value, "true"); writes++; return 1;
	}
	return 0;
}

#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif
#define NUMBEROF(a) ((short)(sizeof(a)/sizeof((a)[0])))
#define MAXIMUM_STRINGS 32
enum {_third_person_player_choice};
static int third_person_policy(void) { return _third_person_player_choice; }
struct widget_instance {
 long definition_tag_index; short type;
 struct widget_instance *parent,*child,*next;
 union {struct {short selected_index,number_of_items;} list;} parameters;
};
struct player_profile {byte fields[8];};
#include "under_test_types.inc"
static struct {char const *name;short field;long default_value;} profile_settings[1]={{"profile.none",0,0}};
static struct player_profile *player_ui_get_edit_player_profile(void) { return NULL; }
static byte *profile_setting_field(struct player_profile *p,short f) { return p->fields+f; }
static boolean profile_setting_boolean(short f) { return FALSE; }
static boolean config_default(const char *n,char *t,unsigned int size) { snprintf(t,size,"%s",!strcmp(n,"display.modern_shadows")?"off":"false");return TRUE; }
static boolean config_text(const char *n,char *t,unsigned int size) { snprintf(t,size,"%s",!strcmp(n,"display.modern_shadows")?shadow_mode:config_boolean(n)?"true":"false");return TRUE; }
static void platform_log(const char *format,...) {}
static int player_link_needed_master(void) { return -1; }
static int player_link_begin(int master) { return 0; }
static void player_link_unlink(void) {}
static struct pc_menu_setting group_setting={7,"lighting.weapon_glow",2,{"false","true"},NONE};
static struct pc_menu_setting vehicle_setting={8,"display.vehicle_lighting",2,{"false","true"},NONE};
static struct pc_menu_setting shadow_setting={9,"display.modern_shadows",3,{"off","hard","soft"},NONE};
static struct pc_menu_setting *pc_menu_setting_get(long i) { return i==7?&group_setting:i==8?&vehicle_setting:i==9?&shadow_setting:NULL; }
static boolean main_menu_is_active(void) { return FALSE; }
static boolean network_game_is_active(void) { return FALSE; }
static void main_goto_main_menu(void) { CHECK(0,"lighting must not reload the frontend"); }
#include "under_test.inc"
int main(int argc, char **argv)
{
	char text[32];
	int i;
	struct widget_instance spinner;
	(void)argc; (void)argv;
	CHECK(lighting_menu_text("display.unknown", text, sizeof(text), 0) == -1,
		"unrelated controls keep their normal route");
	values[2] = 1;
	CHECK(lighting_menu_text("lighting.weapon_glow", text, sizeof(text), 0) == 1 && !strcmp(text, "true"),
		"any explicitly enabled member is shown as ON");
	CHECK(!lighting_menu_write("lighting.weapon_glow", "custom") && !writes, "CUSTOM is not a valid selection");
	CHECK(lighting_menu_text("lighting.weapon_glow", text, sizeof(text), 1) && !strcmp(text, "false") && !writes,
		"defaults are staged without applying before Save");
	CHECK(values[2] && !values[0], "cancelling staged defaults preserves old preferences");
	CHECK(lighting_menu_write("lighting.weapon_glow", "false"), "save off");
	for (i = 0; i < 4; i++) CHECK(!values[i], "off disables every member");
	CHECK(lighting_menu_write("lighting.weapon_glow", "true"), "save on");
	CHECK(lighting_menu_text("lighting.weapon_glow", text, sizeof(text), 0) && !strcmp(text, "true"),
		"reopening reflects saved group");
	CHECK(!values[4] && !values[5], "weapon group does not change shields");
	CHECK(lighting_menu_write("lighting.shield_emitters", "true") && values[4] && values[5], "shield group");
	CHECK(!lighting_menu_write("lighting.weapon_glow", "invalid"), "invalid values do not write");
	memset(&spinner,0,sizeof(spinner));spinner.definition_tag_index=7;spinner.type=2;
	spinner.parameters.list.number_of_items=2;
	values[0]=values[1]=values[3]=0;values[2]=1;writes=0;
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==1,"menu has only OFF and ON");
	spinner.parameters.list.selected_index=1;
	CHECK(!writes&&values[0]==0,"changing spinner is staged");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==1&&!writes,"Cancel/reopen discards staged choice");
	CHECK(setting_default_show(&spinner,&group_setting)&&spinner.parameters.list.selected_index==0&&!writes,"Defaults stages OFF without writes");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==1&&!writes,"Cancel after Defaults preserves preferences");
	spinner.parameters.list.selected_index=0;
	CHECK(setting_changed_save(&spinner,&group_setting)&&writes==4,"Save writes all four group members");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==0,"reopened menu reads persisted OFF members");
	spinner.parameters.list.selected_index=1;
	CHECK(setting_save(&spinner)&&writes==8,"alternate save handler enables group");
	CHECK(setting_default_show(&spinner,&group_setting)&&setting_save(&spinner)&&writes==12,"alternate save handler applies grouped defaults");
	for(i=0;i<4;i++)CHECK(!values[i],"all weapon members default OFF");
	CHECK(values[4]&&values[5],"weapon defaults do not alter another group");
	spinner.definition_tag_index=8;spinner.parameters.list.number_of_items=2;
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==0,"vehicle toggle defaults off independently");
	spinner.parameters.list.selected_index=1;
	CHECK(!values[6]&&setting_load(&spinner)&&spinner.parameters.list.selected_index==0,"Cancel discards staged vehicle lighting");
	spinner.parameters.list.selected_index=1;
	CHECK(setting_changed_save(&spinner,&vehicle_setting)&&values[6],"Save enables standalone vehicle lighting");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==1,"reopen retains standalone selection");
	CHECK(lighting_menu_write("lighting.weapon_glow","false")&&values[6],"Weapon Glow leaves vehicle lighting untouched");
	CHECK(setting_default_show(&spinner,&vehicle_setting)&&values[6],"Defaults stages vehicle lighting without writing");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==1,"Cancel defaults preserves vehicle lighting");
	CHECK(setting_default_show(&spinner,&vehicle_setting)&&setting_save(&spinner)&&!values[6],"Save defaults disables vehicle lighting");
	spinner.definition_tag_index=9;spinner.parameters.list.number_of_items=3;
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==0,"modern shadows default off");
	for(i=1;i<=2;i++) {
		int before=writes;
		spinner.parameters.list.selected_index=(short)i;
		CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==i-1&&writes==before,"Cancel discards shadow edit");
		spinner.parameters.list.selected_index=(short)i;
		CHECK(setting_changed_save(&spinner,&shadow_setting)&&writes==before+1,"Save writes selected shadow mode only");
		CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==i,"reopen retains shadow mode");
	}
	CHECK(!values[6]&&!values[0]&&values[4],"shadow changes do not enable light categories");
	CHECK(setting_default_show(&spinner,&shadow_setting)&&!strcmp(shadow_mode,"soft"),"shadow defaults are staged");
	CHECK(setting_load(&spinner)&&spinner.parameters.list.selected_index==2,"Cancel defaults retains saved Soft");
	CHECK(setting_default_show(&spinner,&shadow_setting)&&setting_changed_save(&spinner,&shadow_setting)&&!strcmp(shadow_mode,"off"),"Save defaults restores Off");
	return 0;
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
