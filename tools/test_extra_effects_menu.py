"""Extra Effects uses the ordinary generated settings screen and pending edits."""
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from harness import function
import port_settings
from test_hud_button_style import run_c

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / "port/assets/menus"
VIDEO = port_settings.PE + "/video_settings"
EXTRA = VIDEO + "/extra_effects"


def generated(folder):
    filename = (port_settings.PE + "/" + folder).replace("/", ".") + ".xml"
    return filename, "\n".join(port_settings.settings_files()[filename])


def widgets(text):
    return {node.get("name"): node for node in ET.fromstring(text).findall("widget")}


def test_extra_effects_is_embedded_and_uses_standard_pending_settings():
    filename, text = generated("video_settings/extra_effects")
    assert (MENUS / "ce" / filename).read_text() == text
    manifest = json.loads((MENUS / "menus.json").read_text())["files"]
    assert manifest.count("ce/" + filename) == 1
    screen = widgets(text)
    spinner = screen[EXTRA + "/shield_look_spinner"]
    assert spinner.get("setting") == "display.shield_look"
    assert spinner.get("strings") == "CLASSIC|ENHANCED"
    assert spinner.get("values") == "classic|enhanced"
    assert spinner.find("on").attrib == {"event": "created", "run": "port setting load"}
    for button, handler in (("defaults", "port settings defaults"), ("ok", "port settings save")):
        actions = screen[EXTRA + "/button_" + button].findall("on")
        assert [(action.get("event"), action.get("run")) for action in actions[:2]] == [
            ("a", handler), ("start", handler)]
    bar = screen[EXTRA + "/button_bar"]
    assert bar.find("child[@widget='common_button_cancel']") is not None
    assert all(action.get("back") == "true" for action in
               screen[EXTRA + "/extra_effects_settings_screen"].findall("on"))
    # The established header, row, help and button positions are shared.
    neighbor = widgets(generated("video_settings/fov_viewmodels")[1])
    for name in ("header_profile_video_settings", "help", "button_bar"):
        current = screen[EXTRA + "/" + name]
        previous = neighbor[VIDEO + "/fov_viewmodels/" + name]
        for key in ("left", "top", "width", "height", "font", "bitmap"):
            assert current.get(key) == previous.get(key)


def test_video_categories_keep_order_and_fit_each_platform():
    filename, text = generated("video_settings")
    assert (MENUS / "ce" / filename).read_text() == text
    screen = widgets(text)
    children = screen[VIDEO + "/options_menu"].findall("child")
    for platform in ("desktop", "android", "web"):
        rows = [child for child in children
                if "/op_" in child.get("widget") and
                (child.get("platform") is None or platform in child.get("platform").split())]
        names = [child.get("widget").rsplit("/", 1)[-1] for child in rows]
        at = names.index("op_fov_viewmodels")
        assert names[at + 1:at + 3] == ["op_extra_effects", "op_dynamic_lighting"]
        help_top = int(screen[VIDEO + "/help"].get("top"))
        for child in rows:
            row = screen[child.get("widget")]
            assert int(child.get("y")) + int(row.get("height")) <= help_top
    target = EXTRA + "/extra_effects_settings_screen"
    assert screen[VIDEO + "/op_extra_effects"].find("on[@event='a']").get("open") == target


def test_shield_look_pending_defaults_cancel_and_save(tmp_path):
    menu = (ROOT / "port/linux/game/menu_functions.c").read_text()
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    entry = re.search(r'\t\{ "display.shield_look",.*? \},', config, re.S).group(0)
    body = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE -1
enum { _config_string, _environment_value, _platform_all };
struct {char const *name; int type; char const *default_value,*environment; int style,platforms;
        char const *comment;} config_settings[] = {
''' + entry + r'''
};
static long config_setting_index(char const *name) {return strcmp(name,"display.shield_look") ? -1 : 0;}
''' + function(config, "config_default") + r'''
static char committed[32]="classic";
static int writes,failed;
struct widget_instance {int definition_tag_index;
 struct{struct{short selected_index,number_of_items;}list;}parameters;};
struct pc_menu_setting {char const *setting;short loaded_index,value_count;char const *values[2];};
static struct pc_menu_setting setting={"display.shield_look",-1,2,{"classic","enhanced"}};
static struct pc_menu_setting *pc_menu_setting_get(int id){(void)id;return &setting;}
static int spinner_item(struct widget_instance *widget){(void)widget;return 0;}
static int setting_text(char const *name,char *out,unsigned int size,int defaults){
 if(defaults)return config_default(name,out,size);
 snprintf(out,size,"%s",committed);return 1;}
static short setting_value_index(struct pc_menu_setting *s,char const *value){short i;
 for(i=0;i<s->value_count;i++)if(!strcmp(s->values[i],value))return i;return 0;}
static int setting_write(char const *name,char const *value){
 assert(!strcmp(name,"display.shield_look"));if(failed)return 0;
 strcpy(committed,value);writes++;return 1;}
static void platform_log(char const *text,...){(void)text;}
''' + "\n".join(function(menu, name) for name in (
        "setting_load", "setting_default_show", "setting_changed_save")) + r'''
int main(void){struct widget_instance w={0};char text[32];int i;
 assert(!strcmp(config_settings[0].environment,"HALO_SHIELD_LOOK"));
 assert(config_default("display.shield_look",text,sizeof(text)) && !strcmp(text,"classic"));
 w.parameters.list.number_of_items=2;
 assert(setting_load(&w) && w.parameters.list.selected_index==0);
 for(i=0;i<20;i++){
  w.parameters.list.selected_index=1;
  assert(!strcmp(committed,"classic") && writes==i*2);
  assert(setting_load(&w) && w.parameters.list.selected_index==0); /* Cancel/reopen. */
  w.parameters.list.selected_index=1;failed=1;
  assert(!setting_changed_save(&w,&setting) && setting.loaded_index==0);failed=0;
  assert(setting_changed_save(&w,&setting) && !strcmp(committed,"enhanced"));
  assert(setting_changed_save(&w,&setting) && writes==i*2+1); /* Unchanged OK. */
  assert(setting_default_show(&w,&setting) && w.parameters.list.selected_index==0);
  assert(!strcmp(committed,"enhanced")); /* Defaults stays pending. */
  assert(setting_load(&w) && w.parameters.list.selected_index==1); /* Cancel Defaults. */
  assert(setting_default_show(&w,&setting));
  assert(setting_changed_save(&w,&setting) && !strcmp(committed,"classic") && writes==i*2+2);
 }
 w.parameters.list.selected_index=-1;assert(setting_changed_save(&w,&setting));
 w.parameters.list.selected_index=2;assert(setting_changed_save(&w,&setting));assert(writes==40);
 return 0;}
'''
    run_c(tmp_path, body)
