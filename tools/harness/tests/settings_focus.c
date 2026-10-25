#include "harness.h"
#include <string.h>
struct widget_instance {
	struct widget_instance *child, *next, *focused_child;
	short type;
	long definition_tag_index;
	char *name;
	struct {
		struct { struct widget_instance *extended_description; } list;
		struct { short string_list_index; } text_box;
	} parameters;
};
static int setting;
static void *pc_menu_setting_get(long tag) { return tag == 7 ? &setting : NULL; }
static short pc_menu_string_index(long tag) { return tag == 6 ? 0 : NONE; }
#include "under_test.inc"
int main(int argc, char **argv)
{
	struct widget_instance list={0}, row={0}, label={0}, spinner={0}, help={0};
	row.name="op_plasma_lighting"; row.child=&label;
	label.type=1; label.definition_tag_index=6; label.next=&spinner;
	spinner.type=2; spinner.definition_tag_index=7;
	list.focused_child=&row; list.parameters.list.extended_description=&help;
	settings_help(&list);
	CHECK(row.focused_child==&spinner,"returning to a row restores its setting input route");
	CHECK(help.parameters.text_box.string_list_index==1,"first setting retains help");
	row.focused_child=&label; settings_help(&list);
	CHECK(row.focused_child==&label,"existing focus is not replaced");
	row.focused_child=NULL; spinner.definition_tag_index=8; settings_help(&list);
	CHECK(!row.focused_child,"unrelated list does not gain setting focus");
	row.name="button_bar"; label.type=0; settings_help(&list);
	CHECK(!row.focused_child && help.parameters.text_box.string_list_index==0,"button bar remains owned by normal navigation");
	list.focused_child=NULL; settings_help(&list);
	CHECK(help.parameters.text_box.string_list_index==0,"no row is safe");
	return 0;
}
